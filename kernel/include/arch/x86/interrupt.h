#include <stdlib.h>

// todo: standardize register structure for interrupts
struct regs;

/**
 * Pointer to the handler function for the interrupt with index in IDT.
 * @index - which IDT vector fired up
 * @regs - pointer to snapshot of CPU state at the time of interrupt
 *
 * @return void
 */
typedef void (*interrupt_handler_t)(uint32_t index, struc regs *regs);

/**
 * Register an interrupt handler to the IDT
 * @idt_index - index of the interrupt to register the handler in the IDT
 * @handler - pointer to the interrupt handler function
 *
 * @return void
 */
void register_interrupt_handlers(uint32_t idt_index,
                                 interrupt_handler_t handler);

/*
 * Unregister an interrupt handler from the IDT
 * @idt_index - index of the interrupt to unregister the handler from IDT
 *
 * @return void
 */
void unregister_interrupt_handlers(uint32_t idt_index);

/**
 * Central Route for registration of all interrupts
 * @idt_index - index if the interrupt to register the handler in IDT
 * regs - pointer to the snapshot of CPU register states at time of interrupt
 */
void isr_common_handler(uint32_t idt_index, struct regs *regs);
