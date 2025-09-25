#ifndef TSS_H
#define TSS_H

#include <stdint.h>


// Double fault stack (4KB)
#define DOUBLE_FAULT_STACK_SIZE 0x1000 
extern uint8_t double_fault_stack[DOUBLE_FAULT_STACK_SIZE];


// Complete TSS structure for task switching
struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

extern struct tss_entry tss_df;
extern void double_fault_handler(void);  // Assembly handler
void init_tss();

#endif // TSS_H

