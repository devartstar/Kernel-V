#include "kernel.h"

void kernel_main() {
    // Initialize kernel subsystems
    printk_init();
    
    // Print welcome message using printk
    printk("%s v%s - Hello Devjit!\n", KERNEL_NAME, KERNEL_VERSION);
    printk("Kernel initialized successfully.\n\n");
    
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
    printk("\nScrolling test:\n");
    for (int i = 0; i < 30; i++) {
        printk("Line %d - Testing kernel scrolling functionality\n", i);
    }
    
    printk("\nKernel-V is running! Welcome to your custom kernel, Devjit!\n");
}
/*
void kernel_main() 
{
    print_string("Hello, from Kernel-V!\n to Devjit.\n", RED_ON_WHITE);
}
*/

