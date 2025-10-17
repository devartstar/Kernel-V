#include <stdbool.h>
#include "tests/test_runner.h"
#include "core/kernel.h"
#include "lib/printk.h"

// Conditionally include headers based on what's available
#ifdef KERNEL_TESTS

// Unit test headers - include if unit tests might be available
#if defined(UNIT_TESTS) || (!defined(UNIT_TESTS) && !defined(PROC_TESTS))
#include "tests/test_printk.h"
#include "tests/test_panik.h"
#endif

// Process test headers - include if proc tests might be available
#if defined(PROC_TESTS) || (!defined(UNIT_TESTS) && !defined(PROC_TESTS))
#include "tests/proc_tests.h"
#endif

// Function declarations for weak linking approach
extern void run_printk_tests(void) __attribute__((weak));
extern void run_printk_scrolling_test(void) __attribute__((weak));
extern void run_panik_unit_tests(void) __attribute__((weak));
extern void create_test_processes(void) __attribute__((weak));

void run_kernel_tests(void) {
    printk("\n==================================================\n");
    printk("Running Kernel Tests...\n");
    printk("==================================================\n");
    
    bool tests_run = false;
    
    // Check if unit test functions are available and run them
    if (run_printk_tests && run_printk_scrolling_test && run_panik_unit_tests) {
        printk("Running Unit Tests...\n");
        run_printk_tests();
        run_printk_scrolling_test();
        run_panik_unit_tests();
        printk("Unit Tests Complete.\n");
        tests_run = true;
    }
    
    // Check if process test functions are available and run them
    if (create_test_processes) {
        printk("Running Process/Integration Tests...\n");
        create_test_processes();
        printk("Process Tests Complete.\n");
        tests_run = true;
    }
    
    if (!tests_run) {
        printk("No test functions found - check linking configuration\n");
    }
    
    printk("\n==================================================\n");
    printk("All Available Tests Completed\n");
    printk("==================================================\n");
}

#else
void run_kernel_tests(void) {
    printk("Tests disabled in this build configuration\n");
}
#endif