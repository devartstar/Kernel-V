#include "tests/test_printk.h"
#include "drivers/vga.h"
#include "lib/printk.h"

void run_printk_tests(void)
{

	//  Test colored log levels
	pr_emerg("Emergency test message\n");
	pr_error("Error test message\n");
	pr_warn("Warning test message\n");
	pr_info("Information test message\n");
	pr_verbose("Debug test message\n");

	//  Regular printk without level (defaults to INFO)
	pr_verbose("Regular printk message (defaults to INFO level)\n\n");

	//  Demonstrate VGA colors
	vga_print_string("Testing different colors:\n", WHITE_ON_BLACK);
	vga_print_string("White on Black\n", WHITE_ON_BLACK);
	vga_print_string("Red on White\n", RED_ON_WHITE);
	vga_print_string("Green on Black\n", GREEN_ON_BLACK);
	vga_print_string("Yellow on Black\n", YELLOW_ON_BLACK);

	//  Test printk formatting
	pr_verbose("\nTesting printk formatting:\n");
	pr_verbose("String: %s\n", "Hello World");
	pr_verbose("Character: %c\n", 'A');
	pr_verbose("Decimal: %d\n", 42);
	pr_verbose("Hexadecimal: 0x%x\n", 255);
	pr_verbose("Pointer: %p\n", (void*)0xDEADBEEF);
}

void run_printk_scrolling_test(void)
{
	pr_verbose("\nScrolling test:\n");
	for (int i = 0; i < 30; i++)
	{
		pr_verbose("Line %d - Testing kernel scrolling functionality\n", i);
	}
}
