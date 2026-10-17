#ifndef PROC_PREEMPT_H
#define PROC_PREEMPT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Hardware-IRQ nesting depth (vectors 32-47 only). Analogue of Linux's
 * preempt_count hardirq bits. Non-zero means "running inside a hardware
 * interrupt handler" -> context switching is forbidden.
 * Software interrupts (int 0x80 syscalls) are NOT counted: a syscall may
 * legitimately block and switch tasks.
 */
extern volatile uint32_t g_hw_irq_depth;

/*
 * Deferred scheduling request. Set from interrupt context (e.g. timer
 * timeslice expiry); acted on at the interrupt-exit preemption point,
 * AFTER the EOI has been sent. Analogue of Linux TIF_NEED_RESCHED.
 * Single CPU -> one global flag is sufficient.
 */
extern volatile bool g_need_resched;

static inline bool in_hardirq(void) { return g_hw_irq_depth != 0; }
static inline void set_need_resched(void) { g_need_resched = true; }

#endif /* PROC_PREEMPT_H */
