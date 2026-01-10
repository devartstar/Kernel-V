#include "arch/x86/interrupt.h"
#include "core/io.h"
#include "lib/printk.h"
#include "time/timer.h"

#define REG_LINE(name, val)                                                    \
    printk("| %10s | 0x%08lx |\n", name, (uint32_t)(val))

// PIC EOI command
#define PIC1_COMMAND 0x20
#define PIC2_COMMAND 0xA0
#define PIC_EOI 0x20

/**
 * An array of interrupt handlers -
 * At index = index of interrupt in IDT.
 * Contains pointer to the handler function to execute.
 */
static interrupt_handler_metadata_t interrupt_handlers[IDT_VECTOR_COUNT] = {0};

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

void pic_send_eoi(uint8_t irq) {
    // If IRQ came from slave PIC (IRQ 8-15), send EOI to both PICs
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    // Always send EOI to master PIC for IRQs 0-15
    outb(PIC1_COMMAND, PIC_EOI);
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

    /* Prevent form dumping registers for timer interrupts (too verbose) */
    // if (idt_index != 32) {
    //     dump_regs(regs);
    // }

    /* Get the interrupt handler metadata and updare it */
    interrupt_handler_metadata_t *interrupt = &interrupt_handlers[idt_index];
    interrupt->hit_count++;
    interrupt->last_tick = tick_count;

    if (interrupt->hit_count == 1 || is_irq_debug_enabled(idt_index)) {
        pr_info("[IDT] Interrupt %lu (%s) fired: (count=%lu, eip=0x%08lx)\n",
                idt_index, interrupt->name, interrupt->hit_count, regs->eip);
    }

    /* Invoke the interrupt handler registered for the interrupt */
    if (interrupt->handler) {
        interrupt->handler(idt_index, regs);
    } else {
        default_interrupt_handler(idt_index, regs);
    }

    /* CRITICAL: Send End-of-Interrupt (EOI) to PIC for hardware interrupts
     * Before ISR Entry the CPU clears the interrupt flag, disabling the
     * interrups. Without EOI further interrupts are disabled.
     */
    if (idt_index >= 32 && idt_index < 48) {
        // Hardware interrupts (IRQs 0-15 are mapped to interrupts 32-47)
        uint8_t irq = idt_index - 32;
        pic_send_eoi(irq);
    }
}
