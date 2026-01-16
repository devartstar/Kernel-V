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

/* Kernel Background loop */
void kernel_main_loop() {
    uint32_t loop_count = 0;

    while (1) {
        // Periodic system maintenance
        if (loop_count % 1000 == 0) {
            pr_info("Kernel main: System heartbeat (loop %d)\n",
                    loop_count / 1000);
        }

        // Yield to other processes - this is KEY for proper scheduling
        yield();

        // Perform kernel maintenance tasks
        // - Handle delayed work queues
        // - System resource cleanup
        // - Check for shutdown requests

        // Power management - halt until next interrupt
        __asm__ __volatile__("hlt");

        loop_count++;
    }
}

void high_stack_entry() {
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

    // ==========================================
    // PROCESS MANAGEMENT INITIALIZATION
    // ==========================================

    //  Initialize Process Management subsystem
    proc_init();
    pr_info("Process management subsystem initialized\n");

    // Convert current kernel execution to a proper schedulable process
    pcb_t *kernel_main = proc_create_kernel_main("kernel_main");
    if (!kernel_main) {
        panik("CRITICAL: Failed to create kernel main process\n");
    }
    pr_info("Kernel main registered as process PID %d\n", kernel_main->pid);

    // ==========================================
    // ENABLE SCHEDULING AND INTERRUPTS
    // ==========================================

    // Enable interrupts - now we can be scheduled
    __asm__ __volatile__("sti");
    pr_info("Scheduling enabled - kernel main is now schedulable\n");

    // Your interrupt testing code (can be removed in production)
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

    // ==========================================
    // KERNEL TESTS (if enabled)
    // ==========================================
#ifdef KERNEL_TESTS
    /* Nested interrupt test */
    test_nested_irq();

    pr_info("Starting kernel tests...\n");
    run_kernel_tests();
    pr_info("All kernel tests completed successfully!\n");
#else
    pr_info("Production build - testing disabled\n");
#endif

    // ==========================================
    // KERNEL MAIN LOOP
    // ==========================================

    pr_info("Kernel main: Entering system management loop\n");
    kernel_main_loop(); // Never returns
}

void kernel_main() {
    /* Console and Logger Initialization */
    printk_init();
    printk("%s v%s - Hello Devjit!\n", KERNEL_NAME, KERNEL_VERSION);
    printk("Kernel-V is running! Welcome to your custom kernel, Devjit!\n");

    /* Debug breadcrumbs (debug build only) */
    DEBUG_DOUBLE_FAULT_BREADCRUMBS();

    /***********************************
     * Initialize core systems modules *
     ***********************************/

    /* Interrupt Descriptor Table Initialization */
    idt_init();

    /* Programmable Interrupt Controller Initialization */
    pic_init();

    /* Task State Segment Initialization */
    init_tss();

    /* Global Descriptor Table Initialization */
    gdt_init();

    /* Debug the Descriptor Tables */
    DEBUG_IDT_GDT_SETUP();

    /* Initialize Hardware Timer */
    pit_init(PIT_DEFAULT_HZ);

    /* Memory Management Setup */
    parse_e820_map();
    DEBUG_KERNEL_E820_MAP();

    /* Physical Memory Manager */
    pmm_init();

    pmm_reserve_memory_region(RESERVED_TYPE_INIT);
    pmm_reserve_memory_region(RESERVED_TYPE_KERNEL);
    pmm_reserve_memory_region(RESERVED_TYPE_BITMAP);

    /* Virtual Memory Management & Paging */
    paging_init();
    DEBUG_PAGE_TABLES();
    pmm_reserve_memory_region(RESERVED_TYPE_PAGE_TABLE);

    /* Update the TSS CR3 register post enabling paging
       CR3 points to the correct page directory post enabling paging */
    update_tss_cr3();

    /* Debug double fault handler setup */
    DEBUG_DOUBLE_FAULT_SETUP();

    /* Map physical memory of new stack region into page tables */
    map_high_stack(KERNEL_STACK_BOTTOM_VIRT, KERNEL_STACK_TOP_VIRT);

    /* Switch to high virtual stack
       Keep a buffer of 16 bits at the top of the stack for safety
       of stack push/pop from calling switch_to_high_stack */
    uint32_t new_stack_ptr = KERNEL_STACK_TOP_VIRT - 16;
    debug_print(
        "About to switch to high virtual stack. New stack pointer: 0x%08x\n",
        PRINT_UINT32(new_stack_ptr));

    /* Update esp to the new high virtual stack top
       Resume execution at the high_stack_entry */
    switch_to_high_stack(new_stack_ptr, high_stack_entry);
}
