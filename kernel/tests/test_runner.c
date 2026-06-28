#include "tests/test_runner.h"
#include "core/kernel.h"
#include "lib/printk.h"
#include "tests/nested_irq.h"
#include "tests/test_devfs.h"
#include "tests/test_devices.h"
#include "tests/test_fd.h"
#include "tests/test_irq.h"
#include "tests/test_mmio.h"
#include "tests/test_pci.h"
#include "tests/test_ramfs.h"
#include "tests/test_spinlock.h"
#include "tests/test_syscall.h"
#include "tests/test_vfs.h"
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
    KLOG_VERBOSE("TEST",
                 "==================================================\n");
    KLOG_VERBOSE("TEST", "Running Kernel Tests...\n");
    KLOG_VERBOSE("TEST",
                 "==================================================\n");

    bool tests_run = false;

    //  Check if unit test functions are available and run them
#ifdef UNIT_TESTS
    KLOG_VERBOSE("TEST", "Running Unit Tests...\n");

    /* Nested interrupt test */
    test_masking_irq();

    run_printk_tests();
    run_printk_scrolling_test();
    run_panik_unit_tests();
    test_mmio_helpers_basic_32b();
    test_mmio_helpers_basic_16b();
    test_mmio_helpers_basic_8b();
    run_spinlock_tests();
    run_irq_tests();
    run_pci_tests();
    run_devices_tests();
    run_vfs_tests();
    run_ramfs_tests();
    run_devfs_tests();
    run_fd_tests();
    run_syscall_tests();

    KLOG_VERBOSE("TEST", "Unit Tests Complete.\n");
    tests_run = true;
#endif

    //  Check if process test functions are available and run them
#ifdef INTEGRATION_TEST
    KLOG_VERBOSE("TEST", "Running Process/Integration Tests...\n");
    create_test_processes();
    KLOG_VERBOSE("TEST", "Process Tests Complete.\n");

    KLOG_VERBOSE("TEST", "Running Usermode Process/Syscall Tests...\n");
    test_usermode_process();
    KLOG_VERBOSE("TEST", "Syscall Tests Completed.\n");

    tests_run = true;
#endif

    if (!tests_run) {
        KLOG_VERBOSE("TEST",
                     "No test functions found - check linking configuration\n");
    }

    KLOG_VERBOSE("TEST",
                 "==================================================\n");
    KLOG_VERBOSE("TEST", "All Available Tests Completed\n");
    KLOG_VERBOSE("TEST",
                 "==================================================\n");
}

#else
void run_kernel_tests(void) {
    KLOG_VERBOSE("TEST", "Tests disabled in this build configuration\n");
}
#endif
