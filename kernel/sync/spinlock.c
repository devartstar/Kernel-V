#include "sync/spinlock.h"
#include "arch/x86/cpu_utils.h"
#include <stdint.h>

static inline uint32_t atomic_xchg_u32(volatile uint32_t *ptr, uint32_t value) {
    __asm__ __volatile__("xchg %0, %1"
                         : "=r"(value), "+m"(*ptr)
                         : "0"(value)
                         : "memory");
    return value;
}

void spin_lock(spinlock_t *lock) {
    while (atomic_xchg_u32(&lock->locked, 1) != 0) {
        while (lock->locked) {
            cpu_relax();
        }
    }
}

void spin_unlock(spinlock_t *lock) {
    __asm__ __volatile__("" ::: "memory");
    lock->locked = 0;
}

irq_flags_t spin_lock_irqsave(spinlock_t *lock) {
    irq_flags_t flags = irq_save();
    spin_lock(lock);
    return flags;
}

void spin_unlock_irqrestore(spinlock_t *lock, irq_flags_t flags) {
    spin_unlock(lock);
    irq_restore(flags);
}
