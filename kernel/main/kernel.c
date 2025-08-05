#include "kernel.h"

void kernel_main() {

    // Initialize kernel subsystems
    printk_init();

    printk("%s v%s - Hello Devjit!\n", KERNEL_NAME, KERNEL_VERSION);
    printk("\nKernel-V is running! Welcome to your custom kernel, Devjit!\n");

    // Parse and display BIOS memory map
    printk("\n==================================================\n");
    printk("Parsing BIOS Memory Map (E820)...\n");
    parse_and_print_e820_map();

    pmm_init();
    void* frame1 = pmm_alloc_frame();
    if (frame1) {
        printk("Allocated frame at address: %p\n", frame1);
    } else {
        printk("Failed to allocate frame\n");
    }

    void* frame2 = pmm_alloc_frame();
    if (frame2) {
        printk("Allocated another frame at address: %p\n", frame2); 
    } else {
        printk("Failed to allocate another frame\n");
    }

    printk("\n==================================================\n");

    
    #ifdef KERNEL_TESTS
    printk("\n==================================================\n");
    printk("Tests Running...\n");
    run_printk_tests();
    run_printk_scrolling_test();
    run_panik_unit_tests();
    printk("\n==================================================\n");
    #endif
}
