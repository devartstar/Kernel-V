#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "core/panik.h"
#include <stdbool.h>
#include <stdint.h>

#define IDT_VECTOR_COUNT 256

/**
 * We get a string of ranges of IRQ enabled for debugging.
 * Set the bitmap for the IRQs enabled for debugging.
 */
void debug_irq_init(void);

/**
 * Check the bitmap if a particular IRQ is enabled for debugging
 */
bool is_irq_debug_enabled(uint32_t idt_index);

/**
 * Unifies context structure for interrupts.
 */
typedef struct regs {
    /* Pushed by pusha (registers in this order)
     * EAX -> ECX -> EDX -> EBX -> ESP (original value before pusha) -> EBP ->
     * ESI -> EDI
     */
    uint32_t edi; /* destination pointer for memory/string operation */
    uint32_t esi; /* source pointer for memory/string operation */
    uint32_t ebp; /* current stack frame */
    uint32_t esp; /* current stack position before pusha*/
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    /* Interrupt vector number */
    uint32_t int_no;

    /* Error code (0 if not present) */
    uint32_t error_code;

    /* Pushed by CPU automatically */
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
 * interrut_handler_metadata - Contains per interrupt info along with handler
 */
typedef struct interrupt_handler_metadata {
    interrupt_handler_t handler; /* Pointer to the interrupt handler */
    const char *name;            /* Interrupt name */
    uint32_t hit_count;          /* Number of times interrupt is triggered */
    uint32_t last_tick;          /* Last tick before the interrupt */
} interrupt_handler_metadata_t;

typedef uint32_t irq_flags_t;

/**
 * Read the Interrupt Flag (IF) from CPUs EFLAGS
 * Disable Maskable Interrupts
 */
static inline irq_flags_t irq_save(void) {
    irq_flags_t flags;
    __asm__ __volatile__("pushf\n"
                         "pop %0\n"
                         "cli"
                         : "=r"(flags)
                         :
                         : "memory");
    return flags;
}

/**
 * Restore the CPU EFLAG state to it was previously
 * Usage: Enter Kernel Critical Section -> flags = irq_save() -> perform
 * opertation -> irq_restore(flags) -> Exit Kernel Critical Section
 */
static inline void irq_restore(irq_flags_t flags) {
    /* "memory" - prevents the compiler from reordering memory accesses across
     * interrupt boundaries */
    __asm__ __volatile__("push %0\n"
                         "popf"
                         :
                         : "r"(flags)
                         : "memory", "cc");
}

/**
 * Check Interrupt Flag (IF) from EFLAG for interrupts enabled or disabled.
 */
static inline bool irq_is_enabled(void) { return (irq_save() & (1 << 9)) != 0; }

/**
 * Assert for checking IRQ Diabled in critical sections, else panik.
 */
static inline void assert_irqs_disabled(void) {
    if (irq_is_enabled()) {
        panik("IRQs enabled in critical section\n");
    }
}

/**
 * Disable a specific irq by setting bit in IMR (Interrupt Mask Register)
 *
 * @irq - Interrupt number to mask
 *
 * @return void
 */
void irq_mask(uint8_t irq);

/**
 * Enable a specific irq by clearing bit in IMR
 *
 * @irq - Interrupt number to unmask
 *
 * @return void
 */
void irq_unmask(uint8_t irq);

/**
 * Register an interrupt handler to the IDT
 * @idt_index - index of the interrupt to register the handler in the IDT
 * @handler - pointer to the interrupt handler function
 *
 * @return void
 */
void register_interrupt_handler(uint32_t idt_index, interrupt_handler_t handler,
                                const char *name);

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
