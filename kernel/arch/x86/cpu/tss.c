#include "arch/x86/tss.h"
#include "core/debug.h"

/**
 * [INFO] Double Fault Recovery
 * CPU is running (esp) -> Someone assign faulty address to esp -> Timer Interrupt fires ->
 * Kernel tries to save CPU context at esp -> Page Fault occurs because esp is invalid -> 
 * Usual Page Faults (kernel tries self recovery) -> Saves CPU context before recovery ->
 * Still trying to push at bad esp -> Double Fault occurs -> Fallback to TSS defined stack.
 */

//  Define the double fault stack
uint8_t double_fault_stack[DOUBLE_FAULT_STACK_SIZE];

//  Double fault TSS instance
struct tss_entry tss_df;

//  Declare the external assembly handler
extern void double_fault_handler(void);

void init_tss()
{
	/* Clear TSS Entry */
	for (int i = 0; i < (int)sizeof(struct tss_entry); i++)
	{
		((uint8_t*)&tss_df)[i] = 0;
	}

	/* CR3 - CPU Control Register 3 stores the physical address of the page directory 
	   Read current CR3 value - THIS WILL BE ZERO because paging is not yet enabled */
	uint32_t current_cr3;
	__asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));

	/* CRITICAL: Set up kernel stack for privilege level transitions */
	tss_df.ss0 = 0x10;  // Kernel data segment
	tss_df.esp0 = (uint32_t)(double_fault_stack + DOUBLE_FAULT_STACK_SIZE - 16);  // Use same stack as DF handler

	/* Use kernel data segment - as a valid stack segment on double fault */
	tss_df.ss = 0x10;

	/* Set the stack pointer to the top of the double fault stack (FRESH STACK) */
	tss_df.esp = (uint32_t)(double_fault_stack + DOUBLE_FAULT_STACK_SIZE);

	/* Set the kernel code segment on double fault */
	tss_df.cs = 0x08;

	/* Set the instruction pointer to the double fault handler */
	tss_df.eip = (uint32_t)double_fault_handler;

	/* EFLAGS - Set IF (Interrupt Enable) flag to allow interrupts in DF handler */
	tss_df.eflags = 0x202;

	/* Legacy systems doing hardware task switching needs cr3 register value 
	   Update the CR3 register value post enabling paging
	   Modrent Software context switch doesn't need it */
	tss_df.cr3 = current_cr3; 

	/* Set all data segment registers to kernel data segment */
	tss_df.ds = tss_df.es = tss_df.fs = tss_df.gs = 0x10;

	debug_module(TSS, "Initialized double fault TSS with ESP0=0x%08x SS0=0x%04x\n",
				 PRINT_UINT32(tss_df.esp0), PRINT_UINT32(tss_df.ss0));
}

void update_tss_cr3(void)
{
	uint32_t current_cr3;
	__asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
	tss_df.cr3 = current_cr3;
}
