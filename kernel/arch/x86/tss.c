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

    // Get current CR3 value (page directory) - THIS WILL BE ZERO!
    uint32_t current_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
    
    // Set up double fault TSS
    tss_df.ss = 0x10;   // Data segment
    tss_df.esp = (uint32_t)(double_fault_stack + DOUBLE_FAULT_STACK_SIZE); // FRESH STACK!
    tss_df.cs = 0x08;   // Code segment
    tss_df.eip = (uint32_t)double_fault_handler;
    tss_df.eflags = 0x202;
    tss_df.cr3 = current_cr3; // This will be updated later
    tss_df.ds = tss_df.es = tss_df.fs = tss_df.gs = 0x10;
}

void update_tss_cr3(void) {
    uint32_t current_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
    tss_df.cr3 = current_cr3;
}
