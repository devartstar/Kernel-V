#include "core/debug.h"
#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "arch/x86/tss.h"
#include "core/debug_funcs.h"
#include "mm/paging.h"
#include "mm/pmm.h"

void check_double_fault_breadcrumbs(void)
{
	if (!DEBUG_ENABLED)
		return;

	uint32_t* magic1 = (uint32_t*)0x15000;
	uint32_t* magic2 = (uint32_t*)0x15004;
	uint32_t* magic3 = (uint32_t*)0x15008;

	debug_print("Checking double fault breadcrumbs:\n");
	debug_print("  Magic1 (0x15000): 0x%08x %s\n",
				PRINT_UINT32(*magic1),
				(*magic1 == 0xDEADBEEF) ? "(FOUND)" : "(not found)");
	debug_print("  Magic2 (0x15004): 0x%08x %s\n",
				PRINT_UINT32(*magic2),
				(*magic2 == 0xCAFEBABE) ? "(FOUND)" : "(not found)");
	debug_print("  Magic3 (0x15008): 0x%08x %s\n",
				PRINT_UINT32(*magic3),
				(*magic3 == 0x12345678) ? "(FOUND)" : "(not found)");
}

void debug_idt_entry(int num)
{
	if (!DEBUG_IDT_GDT)
		return;

	extern idt_entry_t idt[IDT_ENTRIES];

	debug_module(IDT_GDT, "IDT Entry %d:\n", num);
	debug_module(IDT_GDT, "  base_low:  0x%04x\n", PRINT_UINT16(idt[num].base_low));
	debug_module(IDT_GDT, "  base_high: 0x%04x\n", PRINT_UINT16(idt[num].base_high));
	debug_module(IDT_GDT, "  sel:       0x%04x\n", PRINT_UINT16(idt[num].sel));
	debug_module(IDT_GDT, "  always0:   0x%02x\n", PRINT_UINT8(idt[num].always0));
	debug_module(IDT_GDT, "  flags:     0x%02x\n", PRINT_UINT8(idt[num].flags));
	//  Decode flags
	if (idt[num].flags & 0x80)
		debug_module(IDT_GDT, "    Present: YES\n");
	else
		debug_module(IDT_GDT, "    Present: NO\n");

	uint8_t gate_type = idt[num].flags & 0x0F;
	if (gate_type == 0x5)
		debug_module(IDT_GDT, "    Type: Task Gate\n");
	else if (gate_type == 0xE)
		debug_module(IDT_GDT, "    Type: Interrupt Gate\n");
	else
		debug_module(IDT_GDT, "    Type: 0x%x (unknown)\n", gate_type);
}

void debug_gdt_entry(int num)
{
	if (!DEBUG_IDT_GDT)
		return;

	extern struct gdt_entry gdt[4];

	debug_module(IDT_GDT, "GDT Entry %d:\n", num);
	debug_module(IDT_GDT,
				 "  base: 0x%08x\n",
				 PRINT_UINT16((gdt[num].base_high << 24) | 
				 			  (gdt[num].base_middle << 16) |
					 		  gdt[num].base_low));
	debug_module(IDT_GDT,
				 "  limit: 0x%05x\n",
				 PRINT_UINT16(((gdt[num].granularity & 0x0F) << 16) | gdt[num].limit_low));
	debug_module(IDT_GDT, "  access: 0x%02x\n", PRINT_UINT8(gdt[num].access));
	debug_module(IDT_GDT, "  granularity: 0x%02x\n", PRINT_UINT8(gdt[num].granularity));

	//  Decode access byte
	if (gdt[num].access & 0x80)
		debug_module(IDT_GDT, "    Present: YES\n");
	else
		debug_module(IDT_GDT, "    Present: NO\n");

	uint8_t desc_type = (gdt[num].access >> 3) & 0x1;
	if (desc_type == 0)
		debug_module(IDT_GDT, "    Type: System\n");
	else
		debug_module(IDT_GDT, "    Type: Code/Data\n");
}

void debug_tss_contents(void)
{
	if (!DEBUG_TSS)
		return;

	debug_module(TSS, "TSS Contents:\n");
	debug_module(TSS, "  esp: 0x%08x\n", PRINT_UINT32(tss_df.esp));
	debug_module(TSS, "  ss:  0x%04x\n", PRINT_UINT32(tss_df.ss));
	debug_module(TSS, "  cs:  0x%04x\n", PRINT_UINT32(tss_df.cs));
	debug_module(TSS, "  eip: 0x%08x\n", PRINT_UINT32(tss_df.eip));
	debug_module(TSS, "  cr3: 0x%08x\n", PRINT_UINT32(tss_df.cr3));
	debug_module(TSS, "  ds:  0x%04x\n", PRINT_UINT32(tss_df.ds));
}

void test_stack_overflow(int depth)
{
	if (!DEBUG_STACK_HEAP)
		return;

	uint32_t current_esp;
	__asm__ __volatile__("mov %%esp, %0" : "=r"(current_esp));

	debug_module(
		STACK_HEAP, "Stack depth: %d, ESP=0x%08x\n", depth, PRINT_UINT32(current_esp));

	if (current_esp <= KERNEL_STACK_BOTTOM_VIRT + PAGE_SIZE + 0x1000)
	{
		debug_module(STACK_HEAP,
					 "WARNING: Approaching guard page at 0x%08x!\n",
					 KERNEL_STACK_BOTTOM_VIRT);
		debug_module(STACK_HEAP,
					 "Current ESP: 0x%08x, Guard page: 0x%08x\n",
					 PRINT_UINT32(current_esp),
					 PRINT_UINT32(KERNEL_STACK_BOTTOM_VIRT));
		return; //  Stop recursion in debug mode
	}

	test_stack_overflow(depth + 1);
}

void debug_print_esp_args(uint32_t arg1, uint32_t arg2)
{
	debug_module(STACK_HEAP,
				 "switch_to_high_stack: [esp+4]=0x%08x [esp+8]=0x%08x\n",
				 PRINT_UINT32(arg1),
				 PRINT_UINT32(arg2));
}