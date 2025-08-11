#pragma once
#include <stdint.h>

#define IDT_ENTRIES 256

// Location of the IDT in memory
typedef struct
{
    uint16_t limit;     // Size of the IDT in bytes
    uint32_t base;      // Address of the first entry in the IDT
} __attribute__((packed)) idt_ptr_t;

// Each entry in IDT (Interrupt Descriptor Table) is 8 bytes
// CPU reads the IDT entry based on the interrupt vector number
//  LOAD the CS     - segment selector
//  LOAD the EIP    - address of the ISR (Interrupt Service Routine)
//  CS:EIP          - address of the handler code 
typedef struct 
{
    uint16_t base_low;  // Lower 16 bits of ISR address
    uint16_t sel;       // Kernel code segment selector
    uint8_t always0;    // This byte must always be zero

    /* Flags: Present, DPL, Gate Type
    8 bits:
    Bit 7 (P)       : Present (1 = Entry Valid, 0 = not present)
    Bits 6-5 (DPL)  : Descriptor Privilege Level (0 = Kernel, 3 = User) - 
                        min privelege rquired to trigger via INT n
    Bit 4 (S)       : 0 = System, 1 = Code/Data - Always 0 for IDT entries
    Bits 3-0 (Type) : Gate Type (0xE = Interrupt Gate, 0xF = Trap Gate)
    */
    uint8_t flags;

    uint16_t base_high; // Upper 16 bits of ISR address
} __attribute__((packed)) idt_entry_t;

// APIs
void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags);
void idt_init(void);
