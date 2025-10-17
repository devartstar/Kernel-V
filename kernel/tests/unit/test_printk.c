#include "printk.h"
#include "drivers/vga.h"
#include "tests/test_printk.h"

void run_printk_tests(void) {

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

}

void run_printk_scrolling_test(void) {
    printk("\nScrolling test:\n");
    for (int i = 0; i < 30; i++) {
        printk("Line %d - Testing kernel scrolling functionality\n", i);
    }
}
