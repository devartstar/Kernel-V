#include "arch/x86/interrupt.h"
#include <stdint.h>

/* Structure for spinlock */
typedef struct spinlock {
    volatile uint32_t locked;
} spinlock_t;

void spink_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);

irq_flags_t spink_lock_irqsave(spinlock_t *lock);
irq_flags_t spink_lock_irqrestore(spinlock_t *lock, irq_flags_t flags);
