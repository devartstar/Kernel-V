#include "tests/test_spinlock.h"
#include "arch/x86/interrupt.h"
#include "sync/spinlock.h"

#define EFLAG_IF (1u << 9)

int test_spinlock_basic() {
    spinlock_t lock = SPINLOCK_INIT;

    if (lock.locked) {
        KLOG_ERROR("TEST", "SPINLOCK_BASIC initialized in locked state.\n");
        return 0;
    }

    spin_lock(&lock);

    if (!lock.locked) {
        KLOG_ERROR("TEST", "SPINLOCK_BASIC acquiring lock failed.\n");
        return 0;
    }

    spin_unlock(&lock);

    if (lock.locked) {
        KLOG_ERROR("TEST", "SPINLOCK_BASIC releasing lock failed\n");
        return 0;
    }

    KLOG_INFO("TEST", "SPINLOCK_BASIC basic test passed\n");
    return 1;
}

int test_irq_save_restore_enable() {
    /* Set the interrupt flag */
    __asm__ __volatile__("sti" ::: "memory");

    /* check interrupt should be disabled */
    int eflags_before = read_eflags();
    if (!(eflags_before & EFLAG_IF)) {
        KLOG_ERROR(
            "TEST",
            "SPINLOCK_IRQ_ENABLED interrupts are disabled post enabling.\n");
        return 0;
    }

    /* save the cpu flags */
    int eflags_saved = irq_save();

    /* Read the cpu flags to check irq_save correctly disabled interrupts */
    int eflags_test = read_eflags();
    if (eflags_test & EFLAG_IF) {
        KLOG_ERROR(
            "TEST",
            "SPINLOCK_IRQ_ENABLED interrupts are enabled post disabling.\n");
        return 0;
    }

    /* restore the cpu flags with the previously saved eflags */
    irq_restore(eflags_saved);

    /* Read the cpu flags to check irq_restor correctly restored the original
     * eflags. ie. enabled in this test */
    int eflags_after = read_eflags();
    if (!(eflags_after & EFLAG_IF)) {
        KLOG_ERROR("TEST", "SPINLOCK_IRQ_ENABLED interruprs are disabled post "
                           "restore. Should be enabled.\n");
        return 0;
    }

    KLOG_INFO("TEST", "SPINLOCK_IRQ_ENABLED interrupts corrects restored.\n");
    return 1;
}

int test_irq_save_restore_disable() {
    /* Clear the interrupt flag */
    __asm__ __volatile__("cli" ::: "memory");

    /* check interrupt should be disabled */
    int eflags_before = read_eflags();
    if (eflags_before & EFLAG_IF) {
        KLOG_ERROR(
            "TEST",
            "SPINLOCK_IRQ_DISABLED interrupts are enabled post disabling.\n");
        return 0;
    }

    /* save the cpu flags */
    int eflags_saved = irq_save();

    /* Read the cpu flags to check irq_save correctly disabled interrupts */
    int eflags_test = read_eflags();
    if (eflags_test & EFLAG_IF) {
        KLOG_ERROR(
            "TEST",
            "SPINLOCK_IRQ_ENABLED interrupts are enabled post disabling.\n");
        return 0;
    }

    /* restore the cpu flags with the previously saved eflags */
    irq_restore(eflags_saved);

    /* Read the cpu flags to check irq_restor correctly restored the original
     * eflags. ie. disabled in this test */
    int eflags_after = read_eflags();
    if (eflags_after & EFLAG_IF) {
        KLOG_ERROR("TEST", "SPINLOCK_IRQ_ENABLED interruprs are enabled post "
                           "restore. Should be disabled.\n");
        return 0;
    }

    KLOG_INFO("TEST", "SPINLOCK_IRQ_ENABLED interrupts corrects restored.\n");
    return 1;
}

int test_spinlock_irqsave() {
    spinlock_t lock = SPINLOCK_INIT;

    /* Enabled int interrupts to start the test */
    __asm__ __volatile__("sti" ::: "memory");

    /* check interrupt should be disabled */
    int eflags_before = read_eflags();
    if (!(eflags_before & EFLAG_IF)) {
        KLOG_ERROR("TEST",
                   "SPINLOCK_IRQSAVE interrupts are enabled post disabling.\n");
        return 0;
    }

    /* disable and save the cpu flags and acquire lock */
    int eflags_saved = spin_lock_irqsave(&lock);

    /* Read the cpu flags to check irq_save correctly disabled interrupts */
    int eflags_test = read_eflags();
    if (eflags_test & EFLAG_IF) {
        KLOG_ERROR(
            "TEST",
            "SPINLOCK_IRQSAVE  interrupts are enabled post acquiring lock.\n");
        return 0;
    }

    /* restore the cpu flags with the previously saved eflags and release lock
     */
    spin_lock_irqrestore(&lock, eflags_saved);

    /* Read the cpu flags to check irq_restor correctly restored the original
     * eflags. ie. enabled in this test */
    int eflags_after = read_eflags();
    if (!(eflags_after & EFLAG_IF)) {
        KLOG_ERROR("TEST", "SPINLOCK_IRQSAVE interruprs are disabled post "
                           "restore and release lock. Should be enabled.\n");
        return 0;
    }

    KLOG_INFO("TEST", "SPINLOCK_IRQSAVE interrupts corrects restored.\n");
    return 1;
}
