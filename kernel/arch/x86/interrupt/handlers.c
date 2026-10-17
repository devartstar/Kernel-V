#include "arch/x86/interrupt.h"
#include "arch/x86/pic.h"
#include "core/io.h"
#include "lib/printk.h"
#include "proc/preempt.h"
#include "proc/proc.h"
#include "time/timer.h"

#define REG_LINE(name, val)                                                    \
    printk("| %10s | 0x%08lx |\n", name, (uint32_t)(val))

/*
 * An array of interrupt handlers -
 * At index = index of interrupt in IDT.
 * Contains pointer to the handler function to execute.
 */
static interrupt_handler_metadata_t interrupt_handlers[IDT_VECTOR_COUNT] = {0};

/*
 * Initialize the hardware irq depth and if process needs to be rescheduled
 * after timer interrupt handling.
 */
volatile uint32_t g_hw_irq_depth = 0;
volatile bool g_need_resched = false;

void register_interrupt_handler(uint32_t idt_index, interrupt_handler_t handler,
                                const char *name) {
    if (idt_index >= IDT_VECTOR_COUNT) {
        pr_info("[IDT] Error: Tried to register invalid interrupt index %lu\n",
                idt_index);
        return;
    }

    interrupt_handlers[idt_index].handler = handler;
    interrupt_handlers[idt_index].name = name ? name : "unknown";
    interrupt_handlers[idt_index].hit_count = 0;
    interrupt_handlers[idt_index].last_tick = tick_count;

    pr_info("[IDT] Success: Registered handle for Interrupt vector index %lu\n",
            idt_index);
}

void unregister_interrupt_handler(uint32_t idt_index) {
    if (idt_index >= IDT_VECTOR_COUNT) {
        pr_info(
            "[IDT] Error: Tried to unregister invalid interrupt index %lu\n",
            idt_index);
        return;
    }

    interrupt_handlers[idt_index].handler = 0;
    pr_info(
        "[IDT] Success: Unregistered handle for Interrupt vector index %lu\n",
        idt_index);
}

void default_interrupt_handler(uint32_t idt_index, regs_t *regs) {
    (void)regs;
    pr_error("[IDT] Unhandeled Interrupt %lu\n", idt_index);
    // Todo: Halt or Trigger kernel debugger
}

void dump_regs(regs_t *r) {
    printk("\n===========================\n");
    printk("| Register   | Value      |\n");
    printk("---------------------------\n");

    REG_LINE("EAX", r->eax);
    REG_LINE("EBX", r->ebx);
    REG_LINE("ECX", r->ecx);
    REG_LINE("EDX", r->edx);
    REG_LINE("ESI", r->esi);
    REG_LINE("EDI", r->edi);
    REG_LINE("EBP", r->ebp);
    REG_LINE("ESP", r->esp);

    printk("---------------------------\n");
    REG_LINE("INT_NO", r->int_no);
    REG_LINE("ERRCODE", r->error_code);

    printk("---------------------------\n");
    REG_LINE("EIP", r->eip);
    REG_LINE("CS", r->cs);
    REG_LINE("EFLAGS", r->eflags);

    if (r->cs & 0x3) {
        printk("---------------------------\n");
        REG_LINE("USERESP", r->useresp);
        REG_LINE("SS", r->ss);
    }

    printk("===========================\n");
}

void isr_common_handler(regs_t *regs) {
    uint32_t idt_index = regs->int_no;
    uint8_t is_hw_irq = (idt_index >= 32 && idt_index < 48);

    /* update hardware IRQ depthif current is an hardware generated interrupt */
    if (is_hw_irq) {
        g_hw_irq_depth++;
    }

    /* Get the interrupt handler metadata and updare it */
    interrupt_handler_metadata_t *interrupt = &interrupt_handlers[idt_index];
    interrupt->hit_count++;
    interrupt->last_tick = tick_count;

    if (interrupt->hit_count == 1 || is_irq_debug_enabled(idt_index)) {
        KLOG_INFO("IDT", "Interrupt %lu (%s) fired: (count=%lu, eip=0x%08lx)\n",
                  idt_index, interrupt->name, interrupt->hit_count, regs->eip);

        /* Prevent form dumping registers for timer interrupts (too verbose) */
        if (idt_index != 32) {
            dump_regs(regs);
        }
    }

    /* Invoke the interrupt handler registered for the interrupt */
    if (interrupt->handler) {
        interrupt->handler(idt_index, regs);
    } else {
        default_interrupt_handler(idt_index, regs);
    }

    /* CRITICAL: Send End-of-Interrupt (EOI) to PIC for hardware interrupts.
     * Before ISR Entry the CPU clears the interrupt flag, disabling the
     * interrups. Without EOI further interrupts are disabled.
     *
     * NOTE:
     * Acknowledge the hardware IRQ before doing context switch.
     * If we context switch first the EOI is stranded. Signal the EOI first
     * before decising upon switch.
     */
    if (is_hw_irq) {
        // Hardware interrupts (IRQs 0-15 are mapped to interrupts 32-47)
        uint8_t irq = idt_index - 32;
        pic_send_eoi(irq);
        g_hw_irq_depth--;
    }

    /* Check for nested hardware interrupts and if context switch is scheduled
     * by the handler routine.
     * This is Interrupt Exit permption check. Prempt only when:
     * 1. The Handler has requested a reschedule.
     * 2. Execution is not nested inside another Hardware IRQ.
     * 3. The Interrupted context had interrupt enabled. If it was inside a
     * critical section (irq_save()), preemption would brake the critical
     * section.
     */
    if (g_need_resched && !in_hardirq() && current_proc &&
        current_proc->preempt_count == 0 && (regs->eflags & 0x200)) {
        g_need_resched = false;
        yield();

        /* yield() re-enables interrupts after the switch. Disable them again
         * for the rest of the exit path; iretd restores the interrupted
         * context's original EFLAGS, including IF. */
        __asm__ __volatile__("cli");
    }
}
