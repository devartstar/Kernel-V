#include "lib/printk.h"

#define IDT_VECTOR_COUNT 256

#define REG_LINE(name, val) \
    printk("| %-10s | 0x%08x |\n", name, (uint32_t)(val))

/**
 * An array of interrupt handlers -
 * At index = index of interrupt in IDT.
 * Contains pointer to the handler function to execute.
 */
static interrupt_handler_t interrupt_handlers[IDT_VECTOR_COUNT] = {0};

void register_interrupt_handlers(uint32_t idt_index,
                                 interrupt_handler_t handler) {
    if (idt_index > IDT_VECTOR_COUNT) {
        pr_info("[IDT] Error: Tried to register invalid interrupt index %u\n",
                idt_index);
        return;
    }

    interrupt_handlers[idt_index] = handler;
    pr_info("[IDT] Success: Registered handle for Interrupt vector index %u\n",
            idt_index);
}

void unregister_interrupt_handlers(uint32_t idt_index) {
    if (idt_index > IDT_VECTOR_COUNT) {
        pr_info("[IDT] Error: Tried to unregister invalid interrupt index %u\n",
                idt_index);
        return;
    }

    interrupt_handlers[idt_index] = 0;
    pr_info(
        "[IDT] Success: Unregistered handle for Interrupt vector index %u\n",
        idt_index);
}

void dump_regs(regs_t *r)
{
    printk("\n=========================================\n");
    printk("| Register   | Value      |\n");
    printk("-----------------------------------------\n");

    REG_LINE("EAX", r->eax);
    REG_LINE("EBX", r->ebx);
    REG_LINE("ECX", r->ecx);
    REG_LINE("EDX", r->edx);
    REG_LINE("ESI", r->esi);
    REG_LINE("EDI", r->edi);
    REG_LINE("EBP", r->ebp);
    REG_LINE("ESP", r->esp);

    printk("-----------------------------------------\n");
    REG_LINE("INT_NO", r->int_no);
    REG_LINE("ERRCODE", r->error_code);

    printk("-----------------------------------------\n");
    REG_LINE("EIP", r->eip);
    REG_LINE("CS", r->cs);
    REG_LINE("EFLAGS", r->eflags);

    if (r->cs & 0x3) {
        printk("-----------------------------------------\n");
        REG_LINE("USERESP", r->useresp);
        REG_LINE("SS", r->ss);
    }

    printk("=========================================\n");
}

void isr_common_handler(uint32_t idt_index, regs_t *regs) {
    if (interrupt_handlers[idt_index]) {
        dump_regs(regs);
        interrupt_handlers[idt_index](idt_index, regs);
    } else {
        pr_info("[IDT] Error: Cannot handle interrupt, handeler not "
                "initialized for vector index %u\n",
                idt_index);
    }
}
