#include "tests/nested_irq.h"

volatile uint8_t nested_test_count = 0;

void test_interrupt_handler(uint32_t idt_index, regs_t *regs) {
    nested_test_count++;
    printk("[IRQ%u] Test low priority interrupt entered, nesting count: %u\n",
           idt_index, nested_test_count);

    // Get the EFLAGS and disable interrupt.
    irq_flags_t old = irq_save();

    // Enable Interrupts.
    __asm__ __volatile__("sti" ::: "memory");

    // Simulate a long work
    for (volatile uint32_t i = 0; i < 10000000; ++i) {
    }

    // Restore to the state before handler was entered
    irq_restore(old);

    printk("[IRQ%u] Test low peiority interrupt exiting, nesting count: %u\n",
           idt_index, nested_test_count);

    nested_test_count--;
}
