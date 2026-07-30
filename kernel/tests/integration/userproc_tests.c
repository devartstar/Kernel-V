#include "arch/x86/usermode_stub.h"
#include "core/panik.h"
#include "fs/devfs.h"
#include "fs/ramfs.h"
#include "fs/vfs.h"
#include "lib/printk.h"
#include "lib/string.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "mm/stack_map.h"
#include "proc/user.h"
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

/* Fixture file the SYS_OPEN test program expects to open successfully. */
#define OPEN_TEST_FIXTURE_PATH "/hello.txt"

static void spawn_user_test_process(void);
static int ensure_open_test_fixture(void);

/*
 * ensure_open_test_fixture - guarantee the VFS holds the fixture file that the
 * SYS_OPEN user program opens.
 *
 * The VFS is not seeded at boot, and unit tests may or may not have populated
 * it depending on the build configuration. Reuse an existing tree when the
 * fixture is already present; otherwise stand up a minimal VFS and seed the
 * default ramfs layout. Returns non-zero on success.
 */
static int ensure_open_test_fixture(void) {
    if (vfs_lookup_absolute(OPEN_TEST_FIXTURE_PATH) != NULL &&
        vfs_lookup_absolute("/dev/stdin") != NULL) {
        return 1;
    }

    vfs_system_init();
    if (vfs_init() != VFS_OK) {
        KLOG_ERROR("TEST", "open fixture: vfs_init failed\n");
        return 0;
    }
    if (ramfs_seed_root() != VFS_OK) {
        KLOG_ERROR("TEST", "open fixture: ramfs_seed_root failed\n");
        return 0;
    }
    if (devfs_seed_root() != VFS_OK) {
        KLOG_ERROR("TEST", "open fixture: devfs_seed_root failed\n");
        return 0;
    }

    return vfs_lookup_absolute(OPEN_TEST_FIXTURE_PATH) != NULL &&
           vfs_lookup_absolute("/dev/stdin") != NULL;
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

    if (!ensure_open_test_fixture()) {
        KLOG_ERROR("TEST", "skipping SYS_OPEN case: VFS fixture unavailable\n");
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

    pcb_t *proc = userproc_create_from_blob("user_proc_test",
                                            _binary_userprog_start, blob_size);

    if (!proc) {
        panik("spawn_user_test_process: failed");
    }
}
