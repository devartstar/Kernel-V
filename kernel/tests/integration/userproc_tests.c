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

static void map_user_range(uint32_t start, uint32_t end, uint32_t flags) {
    for (uint32_t va = start; va < end; va += PAGE_SIZE) {
        void *phys = pmm_alloc_frame();
        if (!phys) {
            panik("Failed to alloc frame for user range");
        }

        paging_map_page(va, (uint32_t)phys, flags | PAGE_PRESENT | PAGE_USER);
    }
}

void test_usermode_process(void) {
    KLOG_VERBOSE("TEST", "RUNNING usermode process test\n");

    spawn_user_test_process();

    KLOG_VERBOSE("TEST", "User process test completed\n");
}

void spawn_user_test_process(void) {
    uint32_t blob_size =
        (uint32_t)(_binary_userprog_end - _binary_userprog_start);

    pcb_t *proc = userproc_create_from_blob("user_proc_test",
                                            _binary_userprog_start, blob_size);

    if (!proc) {
        panik("spawn_user_test_process: failed");
    }
}
