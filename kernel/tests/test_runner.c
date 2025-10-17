#include "tests/test_runner.h"
#include "core/kernel.h"
#include "lib/printk.h"

#ifdef KERNEL_TESTS
#include "tests/test_printk.h"
#include "tests/test_panik.h"
#endif

void run_kernel_tests(void) {
    #ifdef KERNEL_TESTS
    printk("\n==================================================\n");
    printk("Running Kernel Tests...\n");
    printk("==================================================\n");
    
    run_printk_tests();
    run_printk_scrolling_test();
    run_panik_unit_tests();
    
    printk("\n==================================================\n");
    printk("All Tests Completed\n");
    printk("==================================================\n");
    #else
    // Tests are disabled in release builds
    printk("Tests disabled in this build configuration\n");
    #endif
}