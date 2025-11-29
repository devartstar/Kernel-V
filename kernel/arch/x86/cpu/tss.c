#include "arch/x86/tss.h"
#include "core/debug.h"

//  Define the double fault stack
uint8_t double_fault_stack[DOUBLE_FAULT_STACK_SIZE];

//  Double fault TSS instance
struct tss_entry tss_df;

//  Declare the external assembly handler
extern void double_fault_handler(void);

void init_tss()
{
	//  Clear TSS
	for (int i = 0; i < sizeof(struct tss_entry); i++)
	{
		((uint8_t*)&tss_df)[i] = 0;
	}

	//  Get current CR3 value (page directory) - THIS WILL BE ZERO!
	uint32_t current_cr3;
	__asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));

	//  Set up double fault TSS
	tss_df.ss = 0x10; //  Data segment
	tss_df.esp = (uint32_t)(double_fault_stack +
							DOUBLE_FAULT_STACK_SIZE); //  FRESH STACK!
	tss_df.cs = 0x08;								  //  Code segment
	tss_df.eip = (uint32_t)double_fault_handler;
	tss_df.eflags = 0x202;
	tss_df.cr3 = current_cr3; //  This will be updated later
	tss_df.ds = tss_df.es = tss_df.fs = tss_df.gs = 0x10;
	debug_module(TSS,
				 "Initialized double fault TSS at %p: ss=0x%04x, esp=0x%08x, "
				 "cs=0x%04x, eip=0x%08x, eflags=0x%08x\n",
				 (void*)&tss_df,
				 tss_df.ss,
				 tss_df.esp,
				 tss_df.cs,
				 tss_df.eip,
				 tss_df.eflags);
	debug_module(TSS, "Initialized successfully!\n");
}

void update_tss_cr3(void)
{
	uint32_t current_cr3;
	__asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
	tss_df.cr3 = current_cr3;
}
