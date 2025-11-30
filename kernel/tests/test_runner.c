#include "tests/test_runner.h"
#include "core/kernel.h"
#include "lib/printk.h"
#include <stdbool.h>

//  Conditionally include headers based on what's available
#ifdef KERNEL_TESTS

//  Unit test headers - include if unit tests might be available
#if defined(UNIT_TESTS) || (!defined(UNIT_TESTS) && !defined(PROC_TESTS))
#include "tests/test_panik.h"
#include "tests/test_printk.h"
#endif

//  Process test headers - include if proc tests might be available
#if defined(PROC_TESTS) || (!defined(UNIT_TESTS) && !defined(PROC_TESTS))
#include "tests/proc_tests.h"
#endif

//  Function declarations for weak linking approach
extern void run_printk_tests(void) __attribute__((weak));
extern void run_printk_scrolling_test(void) __attribute__((weak));
extern void run_panik_unit_tests(void) __attribute__((weak));
extern void create_test_processes(void) __attribute__((weak));

void run_kernel_tests(void)
{
	pr_verbose("==================================================\n");
	pr_verbose("Running Kernel Tests...\n");
	pr_verbose("==================================================\n");

	bool tests_run = false;

	//  Check if unit test functions are available and run them
	if (run_printk_tests && run_printk_scrolling_test && run_panik_unit_tests)
	{
		pr_verbose("Running Unit Tests...\n");
		run_printk_tests();
		run_printk_scrolling_test();
		run_panik_unit_tests();
		pr_verbose("Unit Tests Complete.\n");
		tests_run = true;
	}

	//  Check if process test functions are available and run them
	if (create_test_processes)
	{
		pr_verbose("Running Process/Integration Tests...\n");
		create_test_processes();
		pr_verbose("Process Tests Complete.\n");
		tests_run = true;
	}

	if (!tests_run)
	{
		pr_verbose("No test functions found - check linking configuration\n");
	}

	pr_verbose("==================================================\n");
	pr_verbose("All Available Tests Completed\n");
	pr_verbose("==================================================\n");
}

#else
void run_kernel_tests(void)
{
	pr_verbose("Tests disabled in this build configuration\n");
}
#endif