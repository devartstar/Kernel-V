#include "core/kernel.h"
#include "arch/x86/gdt.h"
#include "arch/x86/interrupt.h"
#include "arch/x86/pic.h"
#include "arch/x86/tss.h"
#include "core/debug.h"
#include "core/debug_funcs.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "mm/stack_map.h"
#include "proc/context_switch.h"
#include "proc/proc.h"
#include "tests/proc_tests.h"
#include "tests/test_runner.h"
#include "time/timer.h"

extern void switch_to_high_stack(uint32_t new_esp, void (*entry_func)());

__attribute__((noreturn)) void high_stack_entry() {
    pr_info("Switched to high virtual stack!\n");

    uint32_t cur_esp;
    __asm__ __volatile__("mov %%esp, %0" : "=r"(cur_esp));
    debug_module(STACK_HEAP, "ESP after stack switch: 0x%08x\n",
                 PRINT_UINT32(cur_esp));

    //  Test demand-paged heap access
    debug_module(STACK_HEAP, "Triggering demand-paged heap access...\n");
    volatile int *heap_ptr = (int *)(KERNEL_HEAP_START + 0x1234);
    *heap_ptr = 42;
    debug_module(STACK_HEAP, "Heap page mapped and write succeeded!\n");

    //  Stack overflow testing (debug only)
    if (DEBUG_STACK_HEAP) {
        debug_module(STACK_HEAP, "Testing stack overflow detection...\n");
        pr_info("Current page directory CR3: 0x%08x\n",
                PRINT_UINT32(tss_df.cr3));
    }

    //  VGA memory test
    volatile uint16_t *vga_test = (volatile uint16_t *)0xB8000;
    *vga_test = 0x4F41; //  'A' with white on red
    debug_module(STACK_HEAP, "VGA memory test: wrote to 0xB8000\n");

    //  Initialize Process Management
    proc_init();
    pr_info("Initialized Process Management...\n");

    //  Main kernel loop
    pr_info("Kernel initialization complete. Entering main loop.");

    // Enable interrupts for timer-based preemption
    __asm__ __volatile__("sti");

    /* STEP 1. Show that interrupts are enabled */
    uint32_t eflags_before;
    __asm__ __volatile__("pushf; pop %0" : "=r"(eflags_before));
    if (eflags_before & 0x200) {
        pr_info("Interrupts successfully enabled (EFLAGS=0x%08x, IF bit set)\n",
                PRINT_UINT32(eflags_before));
    } else {
        pr_info("ERROR: Interrupts NOT enabled! (EFLAGS=0x%08x)\n",
                PRINT_UINT32(eflags_before));
    }

    /* STEP 2. Now Disable Interrupts and save state */
    irq_flags_t saved_flags = irq_save();
    printk("[KERNEL] IRQs disabled (should see no more timer output). IF=%d\n",
           irq_is_enabled());

    /* STEP 3. Verify that interrupts are disabled now */
    uint32_t eflags_after;
    __asm__ __volatile__("pushf; pop %0" : "=r"(eflags_after));
    if (eflags_after & 0x200) {
        pr_info("ERROR: Interrupts still enabled after irq_save()! "
                "(EFLAGS=0x%08x)\n",
                PRINT_UINT32(eflags_after));
    } else {
        pr_info("Interrupts successfully disabled (EFLAGS=0x%08x, IF bit "
                "cleared)\n",
                PRINT_UINT32(eflags_after));
    }

    //  Create test processes only if tests are enabled
#ifdef KERNEL_TESTS
    //  Run kernel tests first
    run_kernel_tests();
#else
    pr_info("Production build - testing disabled\n");
#endif

    while (1) {
        __asm__ __volatile__("hlt");
    }
}

void kernel_main() {
    //  Console and Logger Initialization
    printk_init();
    printk("%s v%s - Hello Devjit!\n", KERNEL_NAME, KERNEL_VERSION);
    printk("Kernel-V is running! Welcome to your custom kernel, Devjit!\n");

    //  Debug breadcrumbs (debug build only)
    check_double_fault_breadcrumbs();

    //  Initialize core systems
    idt_init();

    pic_init();

    init_tss();

    gdt_init();

    //  Debug system state
    DEBUG_IDT_GDT_SETUP();

    //  Initialize Timer
    pit_init(PIT_DEFAULT_HZ);

    //  Memory Management Setup
    parse_and_print_e820_map();

    //  Physical Memory Manager
    pmm_init();

    pmm_reserve_memory_region(RESERVED_TYPE_INIT);
    pmm_reserve_memory_region(RESERVED_TYPE_KERNEL);
    pmm_reserve_memory_region(RESERVED_TYPE_BITMAP);

    //  Virtual Memory & Paging
    paging_init();

    //  Debug page tables
    DEBUG_PAGE_TABLES();

    pmm_reserve_memory_region(RESERVED_TYPE_PAGE_TABLE);
    update_tss_cr3();

    //  Debug double fault handler setup
    DEBUG_DOUBLE_FAULT_SETUP();

    //  Map stack region
    map_high_stack(KERNEL_STACK_BOTTOM_VIRT, KERNEL_STACK_TOP_VIRT);

    //  Switch to high virtual stack
    uint32_t new_stack_ptr = KERNEL_STACK_TOP_VIRT - 16;
    debug_print(
        "About to switch to high virtual stack. New stack pointer: 0x%08x\n",
        PRINT_UINT32(new_stack_ptr));
    switch_to_high_stack(new_stack_ptr, high_stack_entry);
}
