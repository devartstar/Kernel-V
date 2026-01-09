#include "arch/x86/gdt.h"
#include "arch/x86/tss.h" // for your tss_entry and extern tss_df
#include "core/debug.h"
#include <stdint.h>

#define GDT_ENTRIES 4
struct gdt_entry gdt[GDT_ENTRIES];
struct gdt_ptr gdtp;

extern void gdt_flush(uint32_t);

static void set_gdt_entry(
	int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
	gdt[num].limit_low = (limit & 0xFFFF);
	gdt[num].base_low = (base & 0xFFFF);
	gdt[num].base_middle = (base >> 16) & 0xFF;
	gdt[num].access = access;
	gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
	gdt[num].base_high = (base >> 24) & 0xFF;

	debug_module(IDT_GDT,
				 "Set GDT entry %d: base=0x%08x, limit=0x%05x, access=0x%02x, "
				 "gran=0x%02x\n",
				 num,
				 PRINT_UINT32(base),
				 PRINT_UINT32(limit),
				 PRINT_UINT8(access),
				 PRINT_UINT8(gran));
}

void gdt_init(void)
{
	set_gdt_entry(0, 0, 0, 0, 0); //  Null
	debug_module(IDT_GDT, "Null entry initialized successfully!\n");
	set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xCF); //  Code seg (0x08)
	debug_module(IDT_GDT, "Kernel Code segment initialized successfully!\n");
	set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xCF); //  Data seg (0x10)
	debug_module(IDT_GDT, "Kernel Data segment initialized successfully!\n");
	set_gdt_entry(3,
				  (uint32_t)&tss_df,
				  sizeof(struct tss_entry) - 1,
				  0x89,
				  0x40); //  TSS (0x18)
	debug_module(TSS, "TSS segment initialized successfully!\n");

	gdtp.limit = sizeof(gdt) - 1;
	gdtp.base = (uint32_t)&gdt;
	gdt_flush((uint32_t)&gdtp);
	pr_info("[GDT] GDT initialized and loaded\n");

	//  Load TSS selector (0x18, 3rd entry)
	__asm__ volatile("ltr %%ax" : : "a"(0x18));

	//  VERIFY TSS IS LOADED
	uint16_t current_tr;
	__asm__ volatile("str %0" : "=r"(current_tr));
	debug_module(
		TSS, "Current Task Register: 0x%04x (should be 0x18)\n", PRINT_UINT16(current_tr));
}
