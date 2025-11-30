#ifndef KERNEL_DEBUG_FUNCS_H
#define KERNEL_DEBUG_FUNCS_H

#include <stdint.h>

//  Debug function declarations
void check_double_fault_breadcrumbs(void);
void debug_idt_entry(int num);
void debug_gdt_entry(int num);
void debug_tss_contents(void);
void test_stack_overflow(int depth);
void debug_print_esp_args(uint32_t arg1, uint32_t arg2);

//  Debug helper macros
#define DEBUG_IDT_GDT_SETUP()                                                  \
	do                                                                         \
	{                                                                          \
		if (DEBUG_ENABLED && DEBUG_IDT_GDT)                                    \
		{                                                                      \
			debug_print(                                                       \
				"\n==================================================\n");     \
			debug_print("DEBUG: IDT and GDT Setup\n");                         \
			debug_idt_entry(8);                                                \
			debug_gdt_entry(3);                                                \
			debug_tss_contents();                                              \
			debug_print(                                                       \
				"==================================================\n");       \
		}                                                                      \
	} while (0)

#define DEBUG_DOUBLE_FAULT_SETUP()                                             \
	do                                                                         \
	{                                                                          \
		if (DEBUG_ENABLED && DEBUG_TSS)                                        \
		{                                                                      \
			debug_print(                                                       \
				"\n==================================================\n");     \
			debug_print("Double fault TSS configured:\n");                     \
			debug_print("  TSS address: 0x%08x\n", PRINT_UINT32((uint32_t)&tss_df));         \
			debug_print("  Handler EIP: 0x%08x\n", PRINT_UINT32(tss_df.eip));                \
			debug_print("  Handler ESP: 0x%08x\n", PRINT_UINT32(tss_df.esp));                \
			debug_print("  Handler CR3: 0x%08x\n", PRINT_UINT32(tss_df.cr3));                \
			debug_print(                                                       \
				"\n==================================================\n");     \
		}                                                                      \
	} while (0)

#define DEBUG_PAGE_TABLES()                                                    \
	do                                                                         \
	{                                                                          \
		if (DEBUG_ENABLED && DEBUG_PAGING)                                     \
		{                                                                      \
			uint32_t* page_dir = (uint32_t*)PAGE_DIR_START_ADDR;               \
			uint32_t* page_table = (uint32_t*)PAGE_TABLE_START_ADDR;           \
			debug_print("[DEBUG_PAGING] Page directory entry 0: 0x%08x\n",     \
						PRINT_UINT32(page_dir[0]));                                          \
			uint32_t vga_page = 0xB8000 / PAGE_SIZE;                           \
			debug_print(                                                       \
				"[DEBUG_PAGING] VGA memory mapping (0xB8000): Page Table "     \
				"Entry %x: 0x%08x\n",                                          \
				PRINT_UINT32(vga_page),                                                      \
				PRINT_UINT32(page_table[vga_page]));                                         \
			if (page_table[vga_page] & PAGE_PRESENT)                           \
			{                                                                  \
				debug_print(                                                   \
					"[DEBUG_PAGING] VGA memory is mapped and present.\n");     \
			}                                                                  \
			else                                                               \
			{                                                                  \
				debug_print("[DEBUG_PAGING] VGA memory is NOT mapped!\n");     \
			}                                                                  \
		}                                                                      \
	} while (0)

#endif /* KERNEL_DEBUG_FUNCS_H */
