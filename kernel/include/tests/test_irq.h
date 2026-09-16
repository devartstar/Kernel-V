#ifndef TEST_IRQ_H
#define TEST_IRQ_H

#include "arch/x86/irq.h"
#include "arch/x86/pic.h"
#include "lib/printk.h"

/**
 * test_irq_registration_basic - test registering the irq line to the
 * test_irq_handler
 * @irq_line - irq line to register the test handler
 */
uint8_t test_irq_registration_basic(uint8_t irq_line);

/**
 * test_irq_duplicate_registration - register an irq line twice with the same
 * handler.
 * @irq_line - irq line to register duplicate handlers
 */
uint8_t test_irq_duplicate_registration(uint8_t irq_line);

static inline void run_irq_tests(void) {
    uint8_t failed_count = 0;

    if (test_irq_registration_basic(5)) {
        KLOG_INFO("TEST", "Basic irq registration test passed.\n");
    } else {
        failed_count++;
    }

    if (test_irq_duplicate_registration(6)) {
        KLOG_INFO("TEST", "Duplicate irq registration test passed.\n");
    } else {
        failed_count++;
    }

    if (failed_count > 0) {
        KLOG_ERROR("TEST", "Irq tests failed count %u.\n", failed_count);
    } else {
        KLOG_INFO("TEST", "All irq tests passed.\n");
    }
}

#endif TEST_IRQ_H
