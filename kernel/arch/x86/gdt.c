#include "arch/x86/gdt.h"
#include "arch/x86/tss.h" // for your tss_entry and extern tss_df
#include <stdint.h>

#define GDT_ENTRIES 4
struct gdt_entry gdt[GDT_ENTRIES];
struct gdt_ptr   gdtp;

extern void gdt_flush(uint32_t);

static void set_gdt_entry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].limit_low    = (limit & 0xFFFF);
    gdt[num].base_low     = (base & 0xFFFF);
    gdt[num].base_middle  = (base >> 16) & 0xFF;
    gdt[num].access       = access;
    gdt[num].granularity  = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].base_high    = (base >> 24) & 0xFF;
}

void gdt_init(void) {
    set_gdt_entry(0, 0, 0, 0, 0);                    // Null
    set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);        // Code seg (0x08)
    set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xCF);        // Data seg (0x10)
    set_gdt_entry(3, (uint32_t)&tss_df, sizeof(struct tss_entry)-1, 0x89, 0x00); // TSS (0x18)

    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint32_t)&gdt;
    gdt_flush((uint32_t)&gdtp);

    // Load TSS selector (0x18, 3rd entry)
    __asm__ volatile("ltr %%ax" : : "a"(0x18));
}
