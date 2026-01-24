#include "arch/x86/gdt.h"
#include "arch/x86/tss.h" // for your tss_entry and extern tss_df
#include "core/debug.h"
#include <stdint.h>

#define GDT_ENTRIES 6
struct gdt_entry gdt[GDT_ENTRIES];
struct gdt_ptr gdtp;

extern void gdt_flush(uint32_t);

static void set_gdt_entry(int num, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran) {
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].access = access;
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].base_high = (base >> 24) & 0xFF;

    debug_module(IDT_GDT,
                 "Set GDT entry %d: base=0x%08x, limit=0x%05x, access=0x%02x, "
                 "gran=0x%02x\n",
                 num, PRINT_UINT32(base), PRINT_UINT32(limit),
                 PRINT_UINT8(access), PRINT_UINT8(gran));
}

void gdt_init(void) {
    /* Null Entry - First entry is always null */
    set_gdt_entry(0, 0, 0, 0, 0);
    KLOG_VERBOSE("IDT_GDT", "Null entry initialized successfully!\n");

    /*
     * Bits 40-47 are Access Bits
     * Bits 45-46 in Access Bits are for DPL - Display Privelege Level
     * DPL = 0 for Kernel and DPL = 3 for User
     */

    /* Kernel Code segment (0x08) */
    set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);
    KLOG_VERBOSE("IDT_GDT", "Kernel Code segment initialized successfully!\n");

    /* Kernel data segment (0x10) */
    set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xCF); //  Data seg (0x10)
    KLOG_VERBOSE("IDT_GDT", "Kernel Data segment initialized successfully!\n");

    /* User code segment (0x18) */
    set_gdt_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);
    KLOG_VERBOSE("IDT_GDT", "User Code segment initialized successfully!\n");

    /* User data segment (0x20) */
    set_gdt_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);
    KLOG_VERBOSE("IDT_GDT", "User Data segment initialized successfully!\n");

    /* Task Stack segment (0x28)
     * Access Bits: 0x89 -> P(1)DPL(00)S(0)E(1)DC(0)RW(0)A(1)
     * [4 Falg Bits][4 Limit Bits] -> 11110000*/
    set_gdt_entry(5, (uint32_t)&tss_df, sizeof(struct tss_entry) - 1, 0x89,
                  0x40);
    KLOG_VERBOSE("TSS", "TSS segment initialized successfully!\n");

    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base = (uint32_t)&gdt;
    gdt_flush((uint32_t)&gdtp);
    KLOG_VERBOSE("GDT", "GDT initialized and loaded\n");

    KLOG_VERBOSE("GDT", "ptr: base=0x%08x limit=0x%04x", gdtp.base, gdtp.limit);
    KLOG_VERBOSE("TSS", "df addr: 0x%08x", (uint32_t)&tss_df);

    //  Load TSS selector (0x28, 5rd entry)
    uint16_t current_tr = 0x28;
    __asm__ volatile("ltr %0" : : "r"(current_tr));

    //  VERIFY TSS IS LOADED
    __asm__ volatile("str %0" : "=r"(current_tr));
    KLOG_VERBOSE("TSS", "Current Task Register: 0x%04x (should be 0x28)\n",
                 PRINT_UINT16(current_tr));
}
