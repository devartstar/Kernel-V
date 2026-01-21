#include "tests/nested_irq.h"

void test_interrupt_handler(uint32_t idt_index, regs_t *regs) {
    nested_interrupt_count++;
    KLOG_VERBOSE(
        "TEST",
        "[IRQ%u] Test low priority interrupt entered, nesting count: %u\n",
        idt_index, nested_interrupt_count);

    // Get the EFLAGS and disable interrupt.
    irq_flags_t old = irq_save();

    // Enable Interrupts.
    __asm__ __volatile__("sti" ::: "memory");

    // Simulate a long work
    KLOG_VERBOSE("TEST",
                 "Waiting for Interrupt IRQ%u to be nested by a higher "
                 "priority interrupt\n",
                 idt_index);
    for (volatile uint32_t i = 0; i < 10000000; ++i) {
    }

    // Restore to the state before handler was entered
    irq_restore(old);

    KLOG_VERBOSE(
        "TEST",
        "[IRQ%u] Test low peiority interrupt exiting, nesting count: %u\n",
        idt_index, nested_interrupt_count);

    nested_interrupt_count--;
}
