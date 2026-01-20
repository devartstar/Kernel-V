#ifndef NESTED_IRQ_H
#define NESTED_IRQ_H

#include "arch/x86/interrupt.h"
#include "lib/printk.h"

/**
 * Handler for a low priority interrupt
 *
 * @idt_index - interrupt vector number (35)
 * @regs - register context
 *
 * @return - void
 */
void test_interrupt_handler(uint32_t idt_index, regs_t *regs);

/**
 * Trigger to the low priority interrup IRQ3 - IDT35
 */
static inline void test_nested_irq(void) { __asm__ __volatile__("int $0x23"); }

#endif
