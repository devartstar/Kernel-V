#ifndef KERNEL_TEST_SPINLOCK_H
#define KERNEL_TEST_SPINLOCK_H

#include "lib/printk.h"
#include <stdint.h>

/**
 * run_spinlock_tests -  wrapper to run all spinlock tests
 */
void run_spinlock_tests();

/**
 * test_spinlock_basic - basic test for spinlocks
 * > initialize a lock and check locked stated be unlocked
 * > acuire a lock and check locked state to be locked
 * > release a lock and check locked state to be unlocked
 */
int test_spinlock_basic(void);

/**
 * test_irq_save_restore_enable - CPU starts with interrupts enabled
 * > Call IRQ save to save the cpu flags
 * > Call IRQ restore to restore the cpu flags
 * > Verify that interrupts are still enabled
 */
int test_irq_save_restore_enable(void);

/**
 * test_irq_save_restore_disable - CPU starts with interrupts disabled
 * > Call IRQ save to save the cpu flags
 * > Call IRQ restore to restore the cpu flags
 * > Verify that interrupts are still disabled
 */
int test_irq_save_restore_disable(void);

/**
 * test_spinlock_irqsave_enable - basic test for spinlock irq save ie. lock can
 * be acquired by normal kernel context and the interrupt context. > Enable the
 * interrupts to start the test. > acquire spin lock and get the cpu flags. >
 * read the flags to check if interrupt disabled. > release spin lock and
 * restore the cpu flags. > read the flags and cpu flags should be restored.
 */
int test_spinlock_irqsave_enable(void);

/**
 * test_spinlock_irqsave_disable - basic test for spinlock irq save ie. lock can
 * be acquired by normal kernel context and the interrupt context. > Disable the
 * interrupts to start the test. > acquire spin lock and get the cpu flags. >
 * read the flags to check if interrupt disabled. > release spin lock and
 * restore the cpu flags. > read the flags and cpu flags should be restored.
 */
int test_spinlock_irqsave_disable(void);

#endif KERNEL_TEST_SPINLOCK_H
