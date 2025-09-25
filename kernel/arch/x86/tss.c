#include "arch/x86/tss.h"

// Define the double fault stack
uint8_t double_fault_stack[DOUBLE_FAULT_STACK_SIZE];

// Double fault TSS instance
struct tss_entry tss_df;

// Declare the external assembly handler
extern void double_fault_handler(void);

void init_tss() {
    // Clear TSS
    for (int i = 0; i < sizeof(struct tss_entry); i++) {
        ((uint8_t*)&tss_df)[i] = 0;
    }
    
    // Set up double fault TSS using the stack from pmm.h
    tss_df.ss = 0x10;   // Data segment
    tss_df.esp = (uint32_t)(double_fault_stack + DOUBLE_FAULT_STACK_SIZE); // Top of stack
    tss_df.cs = 0x08;   // Code segment
    tss_df.eip = (uint32_t)double_fault_handler; // Handler address (assembly version)
    tss_df.eflags = 0x202; // Interrupt flag set
    tss_df.ds = tss_df.es = tss_df.fs = tss_df.gs = 0x10; // Data segments
    
    // esp0/ss0 are for privilege level changes, not needed for double fault task switching
}
