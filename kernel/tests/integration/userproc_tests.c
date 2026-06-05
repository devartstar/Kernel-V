#include "arch/x86/usermode_stub.h"
#include "core/panik.h"
#include "lib/printk.h"
#include "lib/string.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "mm/stack_map.h"
#include "proc/user.h"

extern uint8_t _binary_userprog_start[];
extern uint8_t _binary_userprog_end[];

extern uint8_t _binary_userprog_a_start[];
extern uint8_t _binary_userprog_a_end[];

extern uint8_t _binary_userprog_b_start[];
extern uint8_t _binary_userprog_b_end[];

static void spawn_user_test_process(void);
static void spawn_two_user_test_process(void);

void test_usermode_process(void) {
    KLOG_VERBOSE("TEST", "RUNNING usermode process test\n");

    /* spawn_user_test_process(); */
    spawn_two_user_test_process();

    KLOG_VERBOSE("TEST", "User process test completed\n");
}

static __attribute__((unused)) void spawn_user_test_process(void) {
    uint32_t blob_size =
        (uint32_t)(_binary_userprog_end - _binary_userprog_start);

    pcb_t *proc = userproc_create_from_blob("user_proc_test",
                                            _binary_userprog_start, blob_size);

    if (!proc) {
        panik("spawn_user_test_process: failed");
    }
}

static void spawn_two_user_test_process(void) {
    size_t size_a =
        (uint32_t)(_binary_userprog_a_end - _binary_userprog_a_start);
    size_t size_b =
        (uint32_t)(_binary_userprog_b_end - _binary_userprog_b_start);

    pcb_t *proc_a =
        userproc_create_from_blob("user_a", _binary_userprog_a_start, size_a);
    if (!proc_a) {
        panik("spawn_two_user_test_process: failed to create user process a");
    }

    pcb_t *proc_b =
        userproc_create_from_blob("user_b", _binary_userprog_b_start, size_b);
    if (!proc_b) {
        panik("spawn_two_user_test_process: failed to create user process b");
    }
}
