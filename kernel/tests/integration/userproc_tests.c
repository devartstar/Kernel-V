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

    /* ---------------------------
     * 1. Map USER STACK
     * --------------------------- */
    KLOG_VERBOSE("TEST", "mapping user stack to 0x%08x - 0x%08x\n",
                 USER_STACK_BOTTOM_VIRT, USER_STACK_TOP_VIRT);
    map_high_stack(USER_STACK_BOTTOM_VIRT, USER_STACK_TOP_VIRT,
                   PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    /* Initialize stack memory
     * Value of each byte 0xCC is a machine opcode for INT 3
     * INT 3 - is a software breakpoint interrupt used by debuggers */
    memset((void *)USER_STACK_BOTTOM_VIRT, 0xCC, USER_STACK_SIZE);

    /* _binary_userprog_start and _binary_userprog_end are address of the first
     * and last byte of the userprog provided by the linker. */
    uint32_t prog_size = _binary_userprog_end - _binary_userprog_start;

    /* ---------------------------
     * 2. Map USER CODE page with execute permissions
     * --------------------------- */
    uint32_t stub_size = (uint32_t)prog_size;

    // Map code page as WRITABLE for copying
    void *phys_code = pmm_alloc_frame();
    if (!phys_code) {
        panik("Failed to alloc frame for user code");
    }
    paging_map_page(USER_CODE_VIRT, (uint32_t)phys_code,
                    PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    KLOG_INFO("TEST", "Copying usermode stub (%d bytes) to 0x%08x\n", stub_size,
              USER_CODE_VIRT);

    /* ---------------------------
     * 3. Copy stub into USER memory
     * --------------------------- */
    memcpy((void *)USER_CODE_VIRT, (void *)_binary_userprog_start, stub_size);

    /* ---------------------------
     * 4. Enter user mode
     * --------------------------- */

    uint32_t user_stack = USER_STACK_TOP_VIRT - 4;
    // Align user stack to 16-byte boundary
    uint32_t user_stack_top = (user_stack & ~0xF) - 4;

    current_proc->user_entry = USER_CODE_VIRT;
    current_proc->user_code_size = stub_size;
    current_proc->user_stack_top = user_stack_top;
    current_proc->user_stack_size = USER_STACK_SIZE;

    KLOG_INFO("TEST", "Entering user mode: code=0x%08x stack=0x%08x\n",
              USER_CODE_VIRT, user_stack);

    switch_to_usermode(current_proc->user_entry, current_proc->user_stack_top);

    panik("Returned from usermode (should never happen)");
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
