#include "arch/x86/tss.h"
#include "arch/x86/gdt.h"
#include "core/kernel.h"
#include "core/debug.h"
#include "core/debug_funcs.h"
#include "tests/test_runner.h"
#include "tests/proc_tests.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "mm/stack_map.h"
#include "proc/proc.h"
#include "proc/context_switch.h"
#include "time/timer.h"

extern void switch_to_high_stack(uint32_t new_esp, void (*entry_func)());

__attribute__((noreturn))
void high_stack_entry() {
    pr_info ("Switched to high virtual stack!\n");
    
    uint32_t cur_esp;
    __asm__ __volatile__ ("mov %%esp, %0" : "=r"(cur_esp));
    pr_verbose ("ESP after stack switch: 0x%08x\n", cur_esp);

    // Test demand-paged heap access
    pr_verbose ("Triggering demand-paged heap access...\n");
    volatile int *heap_ptr = (int *)(KERNEL_HEAP_START + 0x1234);
    *heap_ptr = 42;
    pr_verbose("Heap page mapped and write succeeded!\n");

    // Stack overflow testing (debug only)
    if (DEBUG_STACK) {
        pr_verbose("Testing stack overflow detection...\n");
        pr_info("Current page directory CR3: 0x%08x\n", tss_df.cr3);
    }

    // VGA memory test
    volatile uint16_t* vga_test = (volatile uint16_t*)0xB8000;
    *vga_test = 0x4F41; // 'A' with white on red
    pr_verbose("VGA memory test: wrote to 0xB8000\n");

    // Initialize Process Management
    proc_init();
    pr_info("Initialized Process Management...\n");

    // Create test processes only if tests are enabled
    #ifdef KERNEL_TESTS
        // Run kernel tests first
        run_kernel_tests();
    #else
        pr_info ("Production build - testing disabled\n");
    #endif
    
    // Main kernel loop
    pr_info ("\nKernel initialization complete. Entering main loop.\n");
    while (1) {
        __asm__ __volatile__("cli; hlt");
    }
}

void kernel_main() {
    // Console and Logger Initialization
    printk_init();
    printk("%s v%s - Hello Devjit!\n", KERNEL_NAME, KERNEL_VERSION);
    printk("Kernel-V is running! Welcome to your custom kernel, Devjit!\n");

    // Debug breadcrumbs (debug build only)
    check_double_fault_breadcrumbs();

    // Initialize core systems
    idt_init();

    init_tss();

    gdt_init();

    // Debug system state
    DEBUG_IDT_GDT_SETUP();

    // Initialize Timer
    pit_init(PIT_DEFAULT_HZ);

    // Memory Management Setup
    parse_and_print_e820_map();

    // Physical Memory Manager
    pmm_init();

    pmm_reserve_memory_region(RESERVED_TYPE_INIT);
    pmm_reserve_memory_region(RESERVED_TYPE_KERNEL);
    pmm_reserve_memory_region(RESERVED_TYPE_BITMAP);

    // Virtual Memory & Paging
    paging_init();

    // Debug page tables
    DEBUG_PAGE_TABLES();
    
    pmm_reserve_memory_region(RESERVED_TYPE_PAGE_TABLE);
    update_tss_cr3();

    // Debug double fault handler setup
    DEBUG_DOUBLE_FAULT_SETUP();

    // Map stack region
    map_high_stack(KERNEL_STACK_BOTTOM_VIRT, KERNEL_STACK_TOP_VIRT);

    // Switch to high virtual stack
    uint32_t new_stack_ptr = KERNEL_STACK_TOP_VIRT - 16;
    pr_verbose ("About to switch to high virtual stack. New stack pointer: 0x%08x\n", new_stack_ptr);
    switch_to_high_stack(new_stack_ptr, high_stack_entry);
}