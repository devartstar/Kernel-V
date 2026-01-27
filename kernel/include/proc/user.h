#pragma once

#include "arch/x86/interrupt.h"
#include "proc/proc.h"
#include <stdint.h>

/* 64Kb of user space stack */
#define USER_SPACE_STACK_SIZE 0x10000

/* High end and Low end of the user space stack */
#define USER_STACK_TOP_VIRT 0xBFFFF000
#define USER_STACK_BOTTOM_VIRT                                                 \
    (USER_STACK_TOP_VIRT - USER_SPACE_STACK_SIZE) //  0xBFFE0000

int user_stack_init(pcb_t *proc);

void syscall_interrupt_handler(uint32_t idt_index, regs_t *regs);

void switch_to_usermode(uint32_t entry, uint32_t user_stack_top);
