#include "kernel.h"

void run_all_tests() {
    // Print welcome message using printk
    printk("%s v%s - Hello Devjit!\n", KERNEL_NAME, KERNEL_VERSION);
    printk("\nKernel-V is running! Welcome to your custom kernel, Devjit!\n");

    // Test colored log levels
    pr_info("Kernel initialized successfully.\n");
    pr_emerg("Emergency test message\n");
    pr_alert("Alert test message\n");
    pr_crit("Critical test message\n");
    pr_err("Error test message\n");
    pr_warning("Warning test message\n");
    pr_notice("Notice test message\n");
    pr_debug("Debug test message\n");
    
    // Regular printk without level (defaults to INFO)
    printk("Regular printk message (defaults to INFO level)\n\n");
    
    // Demonstrate VGA colors
    vga_print_string("Testing different colors:\n", WHITE_ON_BLACK);
    vga_print_string("White on Black\n", WHITE_ON_BLACK);
    vga_print_string("Red on White\n", RED_ON_WHITE);
    vga_print_string("Green on Black\n", GREEN_ON_BLACK);
    vga_print_string("Yellow on Black\n", YELLOW_ON_BLACK);
    
    // Test printk formatting
    printk("\nTesting printk formatting:\n");
    printk("String: %s\n", "Hello World");
    printk("Character: %c\n", 'A');
    printk("Decimal: %d\n", 42);
    printk("Hexadecimal: 0x%x\n", 255);
    printk("Pointer: %p\n", (void*)0xDEADBEEF);
    
    // Demonstrate scrolling with numbered lines
    // printk("\nScrolling test:\n");
    // for (int i = 0; i < 30; i++) {
        // printk("Line %d - Testing kernel scrolling functionality\n", i);
    // }

    // Run panic unit tests
    // printk("\n==================================================\n");
    // printk("Running Kernel Panic Unit Tests...\n");
    // run_panik_unit_tests();
    // printk("Unit tests completed.\n");
}

void kernel_main() {

    // Initialize kernel subsystems
    printk_init();
    
    // Run all tests
    // run_all_tests();

    // Parse and display BIOS memory map
    printk("\n==================================================\n");
    printk("Parsing BIOS Memory Map (E820)...\n");
    parse_and_print_e820_map();

    printk("\n==================================================\n");
}

/*
void kernel_main() 
{
    vga_print_string("Hello, from Kernel-V!\n to Devjit.\n", RED_ON_WHITE);
}
*/
