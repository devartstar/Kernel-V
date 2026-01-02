#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>

// todo: standardize register structure for interrupts
typedef struct regs {
    // Pushed by pusha (registers in this order)
    // EAX -> ECX -> EDX -> EBX -> ESP (original
    // value before pusha) -> EBP -> ESI -> EDI
    uint32_t edi; // destination pointer for memory/string
                  // operation
    uint32_t esi; // source pointer for memory/string operation
    uint32_t ebp; // current stack frame
    uint32_t esp; // current stack position before pusha
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    // Interrupt vector number
    uint32_t int_no;

    // Error code (0 if not present)
    uint32_t error_code;

    // Pushed by CPU automatically
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t useresp;
    uint32_t ss;
} regs_t;

/**
 * Pointer to the handler function for the interrupt with index in IDT.
 * @index - which IDT vector fired up
 * @regs - pointer to snapshot of CPU state at the time of interrupt
 *
 * @return void
 */
typedef void (*interrupt_handler_t)(uint32_t index, regs_t *regs);

/**
 * Register an interrupt handler to the IDT
 * @idt_index - index of the interrupt to register the handler in the IDT
 * @handler - pointer to the interrupt handler function
 *
 * @return void
 */
void register_interrupt_handler(uint32_t idt_index,
                                interrupt_handler_t handler);

/*
 * Unregister an interrupt handler from the IDT
 * @idt_index - index of the interrupt to unregister the handler from IDT
 *
 * @return void
 */
void unregister_interrupt_handler(uint32_t idt_index);

/**
 * Central Route for registration of all interrupts
 * @idt_index - index if the interrupt to register the handler in IDT
 * regs - pointer to the snapshot of CPU register states at time of interrupt
 */
void isr_common_handler(regs_t *regs);

#endif
