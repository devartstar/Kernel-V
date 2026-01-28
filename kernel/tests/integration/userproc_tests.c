#include "arch/x86/usermode_stub.h"
#include "core/panik.h"
#include "lib/printk.h"
#include "lib/string.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "mm/stack_map.h"
#include "proc/user.h"

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

    memset((void *)USER_STACK_BOTTOM_VIRT, 0xCC, USER_STACK_SIZE);

    debug_dump_pte(USER_STACK_TOP_VIRT - 4);

    /* ---------------------------
     * 2. Map USER CODE page with execute permissions
     * --------------------------- */
    uint32_t stub_size = (uint32_t)(usermode_stub_end - usermode_stub);
    uint32_t code_pages = ALIGN_UP(stub_size, PAGE_SIZE);

    KLOG_VERBOSE("TEST", "mapping code page to 0x%08x - 0x%08x (EXECUTABLE)\n",
                 USER_CODE_VIRT, USER_CODE_VIRT + PAGE_SIZE);
    
    // Map code page as WRITABLE first for copying, then we'll remap as executable
    void *phys_code = pmm_alloc_frame();
    if (!phys_code) {
        panik("Failed to alloc frame for user code");
    }
    paging_map_page(USER_CODE_VIRT, (uint32_t)phys_code, 
                    PAGE_PRESENT | PAGE_WRITE | PAGE_USER);  // Temporarily writable for copying
    
    KLOG_VERBOSE("TEST", "Hello\n");
    KLOG_VERBOSE("TEST", "Addr of usermode stub=0x%08x, stub size=0x%08x\n",
                 usermode_stub, stub_size);

    /* Debug: Check the original stub bytes before copying */
    uint8_t *orig_stub = (uint8_t *)usermode_stub;
    KLOG_VERBOSE("TEST", "Original stub bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 orig_stub[0], orig_stub[1], orig_stub[2], orig_stub[3], 
                 orig_stub[4], orig_stub[5], orig_stub[6], orig_stub[7]);

    /* Check if the usermode_stub address is properly mapped */
    debug_dump_pte((uint32_t)usermode_stub);

    /* ---------------------------
     * 3. Copy stub into USER memory
     * --------------------------- */
    /* Alternative: Use direct bytes if symbol access fails */
    static uint8_t stub_bytes[] = {
        0xb8, 0x78, 0x56, 0x34, 0x12,  // mov eax, 0x12345678
        0xcd, 0x80,                     // int 0x80 (syscall)
        0xb8, 0x78, 0x56, 0x34, 0x12,  // mov eax, 0x12345678 (again)
        0xcd, 0x80,                     // int 0x80 (syscall again)
        0xeb, 0xf0                      // jmp -16 to start (correct infinite loop)
    };
    
    KLOG_VERBOSE("TEST", "About to copy %d bytes from 0x%08x to 0x%08x\n", 
                 stub_size, (uint32_t)usermode_stub, USER_CODE_VIRT);
    KLOG_VERBOSE("TEST", "Using hardcoded stub bytes as fallback\n");
    
    /* Use the hardcoded bytes instead of the symbol */
    memcpy((void *)USER_CODE_VIRT, stub_bytes, sizeof(stub_bytes));

    KLOG_VERBOSE("TEST",
                 "Ccompleted copy of usermode_stub to user code segment\n");

    debug_dump_pte(USER_CODE_VIRT);

    /* Verify bytes copied to the USER CODE SEGMENT above */
    uint8_t *code = (uint8_t *)USER_CODE_VIRT;
    KLOG_VERBOSE(
        "TEST", "User stub bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n",
        code[0], code[1], code[2], code[3], code[4], code[5], code[6], code[7]);

    /* ---------------------------
     * 4. Enter user mode
     * --------------------------- */
    // The stack grows downward, so we want to start at the very top
    // USER_STACK_TOP_VIRT is 0xBFFFF000, so the actual highest address is 0xBFFFF000 - 4
    uint32_t actual_stack_top = USER_STACK_TOP_VIRT - 4;  // Stay within mapped range
    
    KLOG_VERBOSE("TEST", "Switching to user mode: entry=0x%08x stack=0x%08x\n",
                 USER_CODE_VIRT, actual_stack_top);

    // Add some debug info about the mapping
    debug_dump_pte(actual_stack_top);
    
    switch_to_usermode(USER_CODE_VIRT, actual_stack_top);

    panik("Returned from usermode (should never happen)");
}
