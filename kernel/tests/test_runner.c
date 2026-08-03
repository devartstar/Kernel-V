#include "tests/test_runner.h"
#include "core/kernel.h"
#include "lib/printk.h"
#include "tests/nested_irq.h"
#include "tests/test_console.h"
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
#include <stddef.h>
#include <stdint.h>

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

#define KTEST_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

/* One entry in the test registry: a human name, a category (used for the
 * per-test log directory) and the function that runs it. */
typedef struct {
    const char *name;
    const char *category;
    void (*fn)(void);
} ktest_t;

/* Emit a machine-parseable section marker around a test.
 *
 * The host splitter (tools/logs/split_tests.py) keys off these BEGIN/END
 * lines to slice serial.log into one file per test. Everything printed
 * between a BEGIN and its END - including context switches, IRQ handlers and
 * child-process output - lands in that test's file, because tests run
 * sequentially and the serial stream is time-ordered. INFO level is used so
 * markers survive the default CONFIG_TRACE_LEVEL. */
static void ktest_marker(const char *phase, const ktest_t *t, uint32_t sc) {
    KLOG_INFO("KVTEST", "@@KV_TEST %s name=%s cat=%s sc=%lu\n", phase, t->name,
              t->category, (unsigned long)sc);
}

/* Run every test in a registry, wrapping each in BEGIN/END markers and giving
 * each its own trace scope so early lines carry a stable sc. */
static void run_ktests(const ktest_t *tests, size_t count) {
    for (size_t i = 0; i < count; i++) {
        uint32_t sc = log_trace_begin();
        ktest_marker("BEGIN", &tests[i], sc);
        tests[i].fn();
        ktest_marker("END", &tests[i], sc);
    }
}

#ifdef UNIT_TESTS
/* The MMIO helpers return int; wrap them so the registry can hold a uniform
 * void(*)(void) without calling through a mismatched pointer type. */
static void mmio_32b_test(void) { (void)test_mmio_helpers_basic_32b(); }
static void mmio_16b_test(void) { (void)test_mmio_helpers_basic_16b(); }
static void mmio_8b_test(void) { (void)test_mmio_helpers_basic_8b(); }

static const ktest_t unit_tests[] = {
    {"nested_irq_masking", "unit", test_masking_irq},
    {"printk", "unit", run_printk_tests},
    {"printk_scrolling", "unit", run_printk_scrolling_test},
    {"panik", "unit", run_panik_unit_tests},
    {"mmio_32b", "unit", mmio_32b_test},
    {"mmio_16b", "unit", mmio_16b_test},
    {"mmio_8b", "unit", mmio_8b_test},
    {"spinlock", "unit", run_spinlock_tests},
    {"irq", "unit", run_irq_tests},
    {"pci", "unit", run_pci_tests},
    {"devices", "unit", run_devices_tests},
    {"vfs", "unit", run_vfs_tests},
    {"ramfs", "unit", run_ramfs_tests},
    {"devfs", "unit", run_devfs_tests},
    {"fd", "unit", run_fd_tests},
    {"syscall", "unit", run_syscall_tests},
    {"console", "unit", run_console_tests},
};
#endif

#ifdef INTEGRATION_TEST
static const ktest_t integration_tests[] = {
    {"kernel_processes", "integration", create_test_processes},
    {"usermode_syscall", "integration", test_usermode_process},
};
#endif

void run_kernel_tests(void) {
    KLOG_INFO("TEST",
              "==================================================\n");
    KLOG_INFO("TEST", "Running Kernel Tests...\n");
    KLOG_INFO("TEST",
              "==================================================\n");

    bool tests_run = false;

    //  Check if unit test functions are available and run them
#ifdef UNIT_TESTS
    KLOG_INFO("TEST", "Running Unit Tests...\n");
    run_ktests(unit_tests, KTEST_COUNT(unit_tests));
    KLOG_INFO("TEST", "Unit Tests Complete.\n");
    tests_run = true;
#endif

    //  Check if process test functions are available and run them
#ifdef INTEGRATION_TEST
    KLOG_INFO("TEST", "Running Process/Integration Tests...\n");
    run_ktests(integration_tests, KTEST_COUNT(integration_tests));
    KLOG_INFO("TEST", "Integration Tests Complete.\n");
    tests_run = true;
#endif

    if (!tests_run) {
        KLOG_INFO("TEST",
                  "No test functions found - check linking configuration\n");
    }

    KLOG_INFO("TEST",
              "==================================================\n");
    KLOG_INFO("TEST", "All Available Tests Completed\n");
    KLOG_INFO("TEST",
              "==================================================\n");
}

#else
void run_kernel_tests(void) {
    KLOG_VERBOSE("TEST", "Tests disabled in this build configuration\n");
}
#endif
