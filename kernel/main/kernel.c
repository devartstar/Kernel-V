#include "kernel.h"

void kernel_main() {
    // -------------------------------------------------------------------------
    // Console and Logger Initialization
    // -------------------------------------------------------------------------
    printk_init();
    printk("%s v%s - Hello Devjit!\n", KERNEL_NAME, KERNEL_VERSION);
    printk("Kernel-V is running! Welcome to your custom kernel, Devjit!\n");
    
    // -------------------------------------------------------------------------
    // Initializing IDT (Interrupt Descriptor Table)
    // -------------------------------------------------------------------------
    idt_init();
    // enabling hardware interrupts
    __asm__ __volatile__("sti");

    // -------------------------------------------------------------------------
    // Display BIOS Memory Map (E820)
    // -------------------------------------------------------------------------
    printk("\n==================================================\n");
    printk("Parsing BIOS Memory Map (E820)...\n");
    parse_and_print_e820_map();

    // -------------------------------------------------------------------------
    // Physical Memory Manager Setup
    // -------------------------------------------------------------------------
    pmm_init();
    pmm_reserve_memory_region(RESERVED_TYPE_INIT);
    pmm_reserve_memory_region(RESERVED_TYPE_KERNEL);
    pmm_reserve_memory_region(RESERVED_TYPE_BITMAP);

    void* frame1 = pmm_alloc_frame();
    printk(frame1 ? "Allocated frame at address: %p\n" : "Failed to allocate frame\n", frame1);

    void* frame2 = pmm_alloc_frame();
    printk(frame2 ? "Allocated another frame at address: %p\n" : "Failed to allocate another frame\n", frame2);

    // -------------------------------------------------------------------------
    // Virtual Memory & Paging Setup
    // -------------------------------------------------------------------------
    printk("\n==================================================\n");
    printk("Initializing Paging...\n");
    paging_init();
    pmm_reserve_memory_region(RESERVED_TYPE_PAGE_TABLE);
    printk("Paging initialized successfully!\n");

    // -------------------------------------------------------------------------
    // Optional: Trigger a page fault for testing
    // -------------------------------------------------------------------------

    printk("Triggering demand-paged heap access...\n");
    volatile int *heap_ptr = (int *)(KERNEL_HEAP_START + 0x1234);
    *heap_ptr = 42;
    printk("Heap page mapped and write succeeded!\n");

    /*
    printk("\nTriggering page fault...\n");
    volatile int *ptr = (int *)0xDEADBEEF;  // This address is not mapped
    *ptr = 123;                             // Will cause interrupt 14 (page fault)
    */


    // -------------------------------------------------------------------------
    // Optional Unit Tests
    // -------------------------------------------------------------------------
    #ifdef KERNEL_TESTS
    printk("\n==================================================\n");
    printk("Tests Running...\n");
    run_printk_tests();
    run_printk_scrolling_test();
    run_panik_unit_tests();
    printk("==================================================\n");
    #endif
}
