#include "arch/x86/interrupt.h"
#include <stdint.h>

#define SPINLOCK_INIT                                                          \
    { .locked = 0 }

/* Structure for spinlock */
typedef struct spinlock {
    volatile uint32_t locked;
} spinlock_t;

/**
 * spin_lock - Safe only when protected state cannot be acquired from interrupt
 * context on the same cpu, unless interrupts are disabled.
 */
void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);

/**
 * spin_lock_irqsave - When protected state may be acquired from both interrupt
 * context and the normal kernel context
 */
irq_flags_t spin_lock_irqsave(spinlock_t *lock);
irq_flags_t spin_lock_irqrestore(spinlock_t *lock, irq_flags_t flags);

/**
 * read_eflags - reads the cpu flags and return it
 */
static inline uint32_t read_eflags(void) {
    uint32_t flags;
    __asm__ __volatile__("pushf\n\t"
                         "pop %0"
                         : "=r"(flags)
                         :
                         : "memory");

    return flags;
}
