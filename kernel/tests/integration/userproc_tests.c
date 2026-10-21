#include "arch/x86/usermode_stub.h"
#include "core/panik.h"
#include "fs/devfs.h"
#include "fs/ramfs.h"
#include "fs/vfs.h"
#include "fs/vfs_utils.h"
#include "lib/printk.h"
#include "lib/string.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "mm/stack_map.h"
#include "proc/user.h"
#include "tests/syscall_fixtures.h"
#include "tests/syscall_itest.h"

extern uint8_t _binary_userprog_start[];
extern uint8_t _binary_userprog_end[];

extern uint8_t _binary_userprog_a_start[];
extern uint8_t _binary_userprog_a_end[];

extern uint8_t _binary_userprog_b_start[];
extern uint8_t _binary_userprog_b_end[];

extern uint8_t _binary_userprog_syscall_start[];
extern uint8_t _binary_userprog_syscall_end[];

extern uint8_t _binary_userprog_open_start[];
extern uint8_t _binary_userprog_open_end[];

extern uint8_t _binary_userprog_read_start[];
extern uint8_t _binary_userprog_read_end[];

extern uint8_t _binary_userprog_rws_start[];
extern uint8_t _binary_userprog_rws_end[];

extern uint8_t _binary_userprog_stdio_start[];
extern uint8_t _binary_userprog_stdio_end[];

extern uint8_t _binary_userprog_stdin_start[];
extern uint8_t _binary_userprog_stdin_end[];

static void spawn_user_test_process(void);

/* Storage must outlive setup and all concurrent user processes. Separate
 * nodes alone are insufficient if their RAMFS data buffers alias. */
static struct {
    const char *name;
    uint8_t data[SYSCALL_FIXTURE_CAPACITY];
} syscall_fixtures[] = {
    {.name = SYSCALL_OPEN_FIXTURE},
    {.name = SYSCALL_READ_FIXTURE},
    {.name = SYSCALL_STDIO_FIXTURE},
    {.name = SYSCALL_RWS_FIXTURE},
};

/* Prepare every fixture before spawning any case. Reuse an existing VFS
 * without resetting other tests' nodes or the open-file pool. */
static int prepare_syscall_fixtures(void) {
    if (!vfs_get_root()) {
        vfs_system_init();
        if (vfs_init() != VFS_OK) {
            return 0;
        }
    }
    if (devfs_seed_root() != VFS_OK) {
        return 0;
    }

    vfs_node_t *root = vfs_get_root();
    vfs_node_t *dir = vfs_find_child(root, SYSCALL_FIXTURE_DIR);
    if (!dir) {
        dir = vfs_create_node(SYSCALL_FIXTURE_DIR, VFS_NODE_DIR, NULL, NULL);
        if (!dir || vfs_add_child(root, dir) != VFS_OK) {
            return 0;
        }
    }
    if (dir->type != VFS_NODE_DIR) {
        return 0;
    }

    for (uint32_t i = 0;
         i < sizeof(syscall_fixtures) / sizeof(syscall_fixtures[0]); i++) {
        const char *name = syscall_fixtures[i].name;
        uint8_t *data = syscall_fixtures[i].data;
        uint32_t size = sizeof(SYSCALL_FIXTURE_INITIAL) - 1;
        vfs_node_t *node = vfs_find_child(dir, name);
        if (!node) {
            node =
                ramfs_create_file(name, data, size, SYSCALL_FIXTURE_CAPACITY);
            if (!node || vfs_add_child(dir, node) != VFS_OK) {
                return 0;
            }
        }

        /* Fail rather than adopt an unrelated node or reset an open file. */
        if (node->type != VFS_NODE_FILE || node->ops != &ramfs_file_ops ||
            !node->private_data || node->refcount != 1) {
            return 0;
        }
        ramfs_file_t *file = node->private_data;
        if (file->data != data ||
            file->capacity != SYSCALL_FIXTURE_CAPACITY) {
            return 0;
        }
        memset(data, 0, SYSCALL_FIXTURE_CAPACITY);
        memcpy(data, SYSCALL_FIXTURE_INITIAL, size);
        file->size = node->size = size;
    }
    return 1;
}

/*
 * Syscall integration suite.
 *
 * Each entry is an embedded user program that runs in user mode, makes real
 * syscalls, and reports its verdict through its SYS_EXIT code (0 == success).
 * The generic harness (syscall_itest_run_all) spawns them, waits for them to
 * terminate, and verifies each exit code. To grow the suite, add a new
 * self-checking blob and one row here.
 */
void test_usermode_process(void) {
    KLOG_VERBOSE("TEST", "RUNNING usermode process / syscall tests\n");

    if (!prepare_syscall_fixtures()) {
        KLOG_ERROR("TEST",
                   "[FAIL] syscall fixture setup failed; no cases spawned\n");
        return;
    }

    const syscall_itest_case_t cases[] = {
        {
            .name = "syscall_selftest",
            .blob_start = _binary_userprog_syscall_start,
            .blob_end = _binary_userprog_syscall_end,
            .expected_exit_code = 0,
        },
        {
            .name = "user_a",
            .blob_start = _binary_userprog_a_start,
            .blob_end = _binary_userprog_a_end,
            .expected_exit_code = 0,
        },
        {
            .name = "user_b",
            .blob_start = _binary_userprog_b_start,
            .blob_end = _binary_userprog_b_end,
            .expected_exit_code = 0,
        },
        {
            .name = "syscall_open",
            .blob_start = _binary_userprog_open_start,
            .blob_end = _binary_userprog_open_end,
            .expected_exit_code = 0,
        },
        {
            .name = "syscall_read",
            .blob_start = _binary_userprog_read_start,
            .blob_end = _binary_userprog_read_end,
            .expected_exit_code = 0,
        },
        {
            .name = "syscall_stdio",
            .blob_start = _binary_userprog_stdio_start,
            .blob_end = _binary_userprog_stdio_end,
            .expected_exit_code = 0,
        },
        {
            .name = "syscall_rws",
            .blob_start = _binary_userprog_rws_start,
            .blob_end = _binary_userprog_rws_end,
            .expected_exit_code = 0,
        },
        {
            .name = "syscall_stdin",
            .blob_start = _binary_userprog_stdin_start,
            .blob_end = _binary_userprog_stdin_end,
            .expected_exit_code = 0,
        },
    };

    uint32_t failed =
        syscall_itest_run_all(cases, sizeof(cases) / sizeof(cases[0]));

    if (failed > 0) {
        KLOG_ERROR("TEST", "usermode syscall tests failed: %u case(s).\n",
                   failed);
    }

    KLOG_VERBOSE("TEST", "User process / syscall tests completed\n");
}

/* spawn_user_test_process - spawn a single user blob without verification.
 * Retained for ad-hoc manual bring-up; not part of the verified suite. */
static __attribute__((unused)) void spawn_user_test_process(void) {
    uint32_t blob_size =
        (uint32_t)(_binary_userprog_end - _binary_userprog_start);

    pcb_t *proc = userproc_create_from_blob(
        "user_proc_test", _binary_userprog_start, blob_size);

    if (!proc) {
        panik("spawn_user_test_process: failed");
    }
}
