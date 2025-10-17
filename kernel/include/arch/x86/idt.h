#pragma once
#include <stdint.h>

/*
IDT (Interrupt Descriptor Table):
    data structure used by the CPU to determine how to handle interrupts.
    - It is stored in RAM (memory)
    - CPU only stores the base address + limit in the IDTR (Interrupt Descriptor Table Register).

When an interrupt occurs:
    CPU accesses the IDT entry using the base address and interrupt vector number.
*/

#define IDT_ENTRIES 256

// Location of the IDT in memory
typedef struct
{
    uint16_t limit;     // Size of the IDT in bytes
    uint32_t base;      // Address of the first entry in the IDT
} __attribute__((packed)) idt_ptr_t;

/*
Each entry in IDT (Interrupt Descriptor Table) is 8 bytes
CPU reads the IDT entry based on the interrupt vector number
    LOAD the CS     - segment selector
    LOAD the EIP    - address of the ISR (Interrupt Service Routine)
    CS:EIP          - address of the handler code 
*/
typedef struct 
{
    uint16_t base_low;  // Lower 16 bits of ISR address
    uint16_t sel;       // Kernel code segment selector
    uint8_t always0;    // This byte must always be zero

    /* 
    Flags[8 bits]: Present, DPL, Gate Type
        Bit 7 (P)       :   Present Bit
                            1 Entry is valid and present
                            0 Entry is not valid (not present)

        Bits 5-6 (DPL)  :   Descriptor Privilege Level Bit
                            (0 = Kernel, 3 = User)
                            Min privilege required to trigger interrupt via INT n (Software Interrupt)
                            Ignored for hardware interrupts

        Bit 4 (S)       :   Storage Segment Bit
                            Always 0 for IDT
                            Used in GDT - 0 = System, 1 = Code/Data Segments

        Bit 0-3 (Type)  :   Gate Type Bit
                            Tells the CPU how to transfer control
                            0x5     - Task Gate (Instead of jumping to ISR, it switches task)
                            0x6|0xE - Interrupt Gate (Jumps to ISR, disables hardware interrupts)
                            0x7|0xF - Trap Gate (Jumps to ISR, does not disable hardware interrupts)
                            0x6, 0x7 are for 16 bits | 0xE, 0xF are for 32 bits
    */
    uint8_t flags;

    uint16_t base_high; // Upper 16 bits of ISR address
} __attribute__((packed)) idt_entry_t;

// APIs
void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags);
void idt_init(void);
