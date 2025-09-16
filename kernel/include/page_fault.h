#pragma once
#include <stdint.h>

typedef struct {
    uint32_t ds;                                        // Data segment selector
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;    // Pushed by pusha
    uint32_t int_no;                                    // Interrupt number
    uint32_t error_code;                                // Error code pushed by CPU
    uint32_t eip, cs, eflags, useresp, ss;              // Pushed by CPU on interrupt
} page_fault_stack_t;

void page_fault_handler(page_fault_stack_t* frame);
