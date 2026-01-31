#include "tests/nested_irq.h"

/** Test Cases:
 * 1. IRQ of lower priority is nested by IRQ of higher priority. All IRQ are
 * unmasked.
 * Result: High priority IRQ will nest the Low priority IRQ.
 *
 * 2. Interrupts are enabled. Mask the timer interrupt. Trigger the test
 * interrupt
 * Result: Timer interrupt would not nest the low priority test interrupt.
 */

void test_interrupt_handler(uint32_t idt_index, regs_t *regs) {
    (void)regs;

    nested_interrupt_count++;
    KLOG_VERBOSE(
        "TEST",
        "[IRQ%u] Test low priority interrupt entered, nesting count: %u\n",
        idt_index, nested_interrupt_count);

    // Get the EFLAGS and disable interrupt.
    irq_flags_t old = irq_save();

    // Enable Interrupts.
    __asm__ __volatile__("sti" ::: "memory");

    KLOG_VERBOSE("TEST",
                 "Waiting for Interrupt IRQ%u to be nested by a higher "
                 "priority interrupt\n",
                 idt_index);

    // Simulate a long work
    for (volatile uint32_t i = 0; i < 10000000; ++i) {
    }

    // Restore to the state before handler was entered
    irq_restore(old);

    nested_interrupt_count--;

    KLOG_VERBOSE(
        "TEST",
        "[IRQ%u] Test low peiority interrupt exiting, nesting count: %u\n",
        idt_index, nested_interrupt_count);
}

void test_masking_irq(void) {
    printk("========= TEST MASKING IRQ =========\n");

    KLOG_VERBOSE("TEST",
                 "Masking High Priority Timer Interrupt (IRQ32) and trigger "
                 "Test Interrupt (IRQ35)\n");
    irq_mask(0);
    test_nested_irq();
    KLOG_VERBOSE("TEST", "Expectation: Timer interrupt should not fire until "
                         "test interrupt is completed\n");

    KLOG_VERBOSE("TEST",
                 "Unmasking High Priority Timer Interrupt (IRQ32) and trigger "
                 "Test Interrupt (IRQ35)\n");
    irq_unmask(0);
    test_nested_irq();
    KLOG_VERBOSE(
        "TEST",
        "Expectation: Timer interrupt should nest the test interrupt\n");

    printk("========= TEST MASKING IRQ =========\n");
}
