#include "idt.h"
#include <string.h>
#include <stdint.h>
#include "arch/x86/tss.h"

extern void idt_flush(uint32_t);

// IDT (Interrupt Descriptor Table) Declaration
idt_entry_t idt[IDT_ENTRIES];
idt_ptr_t idt_ptr;

/*
Function to set an IDT entry
    num: Interrupt vector number
    base: Address of the ISR (Interrupt Service Routine)
    sel: Kernel code segment selector
    flags: Flags for the IDT entry (Present, DPL, Gate Type)
*/
void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt[num].base_low  = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel       = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

// Set up a task gate for double fault (interrupt 8)
void set_task_gate(uint8_t num, uint16_t sel) {
    idt[num].base_low = 0;
    idt[num].base_high = 0;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = 0x85; // Present, DPL=0, Task Gate (type 5)
}

/*
Initialize the IDT
*/
void idt_init ()
{
    idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
    idt_ptr.base = (uint32_t)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt[i].base_low  = 0;
        idt[i].base_high = 0;
        idt[i].sel       = 0;
        idt[i].always0   = 0;
        idt[i].flags     = 0;
    }

    // Set up double fault as task gate (TSS selector is 0x18 - 3rd entry in GDT)
    set_task_gate(8, 0x18);

    extern void isr_page_fault();
    // add entry for page fault handler in idt
    // P=1(Present), DPL=0(Kernel only access), Type=0xE(Interrupt Gate)
    idt_set_gate(14, (uint32_t)isr_page_fault, 0x08, 0x8E); 

    idt_flush((uint32_t)&idt_ptr);
}
