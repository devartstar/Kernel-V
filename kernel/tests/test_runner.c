#include "tests/test_runner.h"
#include "core/kernel.h"
#include "lib/printk.h"
#include "tests/nested_irq.h"
#include <stdbool.h>

//  Conditionally include headers based on what's available
#ifdef KERNEL_TESTS

//  Unit test headers - include if unit tests might be available
#ifdef UNIT_TESTS
#include "tests/test_panik.h"
#include "tests/test_printk.h"
#endif

//  Process test headers - include if proc tests might be available
#ifdef INTEGRATION_TEST
#include "tests/proc_tests.h"
#endif

void run_kernel_tests(void) {
    pr_verbose("==================================================\n");
    pr_verbose("Running Kernel Tests...\n");
    pr_verbose("==================================================\n");

    bool tests_run = false;

    //  Check if unit test functions are available and run them
#ifdef UNIT_TESTS
    KLOG_VERBOSE("TEST", "Running Unit Tests...\n");

    /* Nested interrupt test */
    test_nested_irq();

    run_printk_tests();
    run_printk_scrolling_test();
    run_panik_unit_tests();
    pr_verbose("Unit Tests Complete.\n");
    tests_run = true;
#endif

    //  Check if process test functions are available and run them
#ifdef INTEGRATION_TEST
    pr_verbose("Running Process/Integration Tests...\n");
    create_test_processes();
    pr_verbose("Process Tests Complete.\n");
    tests_run = true;
#endif

    if (!tests_run) {
        pr_verbose("No test functions found - check linking configuration\n");
    }

    pr_verbose("==================================================\n");
    pr_verbose("All Available Tests Completed\n");
    pr_verbose("==================================================\n");
}

#else
void run_kernel_tests(void) {
    pr_verbose("Tests disabled in this build configuration\n");
}
#endif
