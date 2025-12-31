#include "lib/printk.h"

#define IDT_VECTOR_COUNT 256

/**
 * An array of interrupt handlers -
 * At index = index of interrupt in IDT.
 * Contains pointer to the handler function to execute.
 */
static interrupt_handler_t interrupt_handlers[IDT_VECTOR_COUNT] = {0};

void register_interrupt_handlers(uint32_t idt_index,
                                 interrupt_handler_t handler) {
    if (idt_index > IDT_VECTOR_COUNT) {
        pr_info("[IDT] Error: Tried to register invalid interrupt index %u\n",
                idt_index);
        return;
    }

    interrupt_handlers[idt_index] = handler;
    pr_info("[IDT] Success: Registered handle for Interrupt vector index %u\n",
            idt_index);
}

void unregister_interrupt_handlers(uint32_t idt_index) {
    if (idt_index > IDT_VECTOR_COUNT) {
        pr_info("[IDT] Error: Tried to unregister invalid interrupt index %u\n",
                idt_index);
        return;
    }

    interrupt_handlers[idt_index] = 0;
    pr_info(
        "[IDT] Success: Unregistered handle for Interrupt vector index %u\n",
        idt_index);
}

void isr_common_handler(uint32_t idt_index, struct regs *regs) {
    if (interrupt_handlers[idt_index]) {
        interrupt_handlers[idt_index](idt_index, regs);
    } else {
        pr_info("[IDT] Error: Cannot handle interrupt, handeler not "
                "initialized for vector index %u\n",
                idt_index);
    }
}
