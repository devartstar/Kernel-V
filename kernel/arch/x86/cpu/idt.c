#include "arch/x86/idt.h"
#include "arch/x86/interrupt.h"
#include "arch/x86/page_fault.h"
#include "arch/x86/tss.h"
#include "core/debug.h"
#include "core/debug_funcs.h"
#include "proc/user.h"
#include "tests/nested_irq.h"
#include "time/timer.h"
#include <stdint.h>
#include <string.h>

extern void idt_flush(uint32_t);
extern void timer_interrupt_handler(uint32_t idt_index, regs_t *regs);
extern void pagefault_interrupt_handler(uint32_t idt_index, regs_t *regs);
extern void test_interrupt_handler(uint32_t idt_index, regs_t *regs);
extern void syscall_interrupt_handler(uint32_t idt_index, regs_t *regs);

//  IDT (Interrupt Descriptor Table) Declaration
idt_entry_t idt[IDT_ENTRIES];
idt_ptr_t idt_ptr;

/*
Function to set an IDT entry
    num: Interrupt vector number
    base: Address of the ISR (Interrupt Service Routine)
    sel: Kernel code segment selector
    flags: Flags for the IDT entry (Present, DPL, Gate Type)
*/
void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;

    debug_module(
        IDT_GDT, "Set IDT gate %d: base=0x%08x, sel=0x%04x, flags=0x%02x\n",
        num, PRINT_UINT32(base), PRINT_UINT16(sel), PRINT_UINT8(flags));
}

//  Set up a task gate for double fault (interrupt 8)
void set_task_gate(uint8_t num, uint16_t sel) {
    idt[num].base_low = 0;  //  Task gates don't use base addresses
    idt[num].base_high = 0; //  They use TSS selector instead
    idt[num].sel = sel;     //  TSS selector (0x28)
    idt[num].always0 = 0;
    idt[num].flags = 0x85; //  Present(1) + DPL(00) + Type(0101 = Task Gate)

    //  DEBUG: Verify the setup
    debug_module(TSS, "Set task gate %d: sel=0x%04x, flags=0x%02x\n", num,
                 PRINT_UINT16(sel), PRINT_UINT8(idt[num].flags));
}

/*
Initialize the IDT
*/
void idt_init() {

    debug_irq_init();

    idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
    idt_ptr.base = (uint32_t)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt[i].base_low = 0;
        idt[i].base_high = 0;
        idt[i].sel = 0;
        idt[i].always0 = 0;
        idt[i].flags = 0;
    }

    //  Set up double fault as task gate (TSS selector is 0x28 - 5rd entry in
    //  GDT)
    set_task_gate(8, 0x28);
    debug_module(TSS, "Task gate initialized successfully in IDT!\n");

    register_interrupt_handler(14, pagefault_interrupt_handler, "PAGE_FAULT");
    extern void isr_stub_14();
    //  add entry for page fault handler in idt
    //  P=1(Present), DPL=0(Kernel only access), Type=0xE(Interrupt Gate)
    idt_set_gate(14, (uint32_t)isr_stub_14, 0x08, 0x8E);
    debug_module(IDT_GDT, "[IDT] Page Fault Entry Initialized successfully!\n");
    DEBUG_PAGEFAULT_INTERRUPTS();

    /* Set up IDT entry for hardware Timer Interrupts
       IRQ 0 -> entry 32 in IDT
     */
    register_interrupt_handler(32, timer_interrupt_handler, "TIMER");
    extern void isr_stub_32();
    idt_set_gate(32, (uint32_t)isr_stub_32, 0x08, 0x8E);
    irq_unmask(0);
    debug_module(IDT_GDT, "[IDT] Timer Entry Initialized successfully!\n");

#ifdef KERNEL_TESTS
    /* Registering a test interrupt handler */
    register_interrupt_handler(35, test_interrupt_handler, "TEST");
    extern void isr_stub_35();
    idt_set_gate(35, (uint32_t)isr_stub_35, 0x08, 0x8E);
    irq_unmask(3);
    debug_module(IDT_GDT, "[IDT] Test Entry Initialized successfully!\n");
#endif

    /* Set up IDT entry for syscalls */
    register_interrupt_handler(128, syscall_interrupt_handler, "SYSCALL");
    extern void isr_stub_128();
    idt_set_gate(128, (uint32_t)isr_stub_128, 0x08, 0xEE);
    debug_module(IDT_GDT, "[IDT] Syscall Entry Initialized successfully!\n");

    idt_flush((uint32_t)&idt_ptr);
    pr_info("[IDT] Loaded successfully!\n");
}
