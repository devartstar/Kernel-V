#pragma once

#include "arch/x86/interrupt.h"
#include "proc/proc.h"
#include <stdint.h>

/* 4Kb of user space stack */
#define USER_SPACE_STACK_SIZE 4096

/* High end and Low end of the user space stack */
#define USER_STACK_TOP_VIRT 0xBFFFF000
#define USER_STACK_BOTTOM_VIRT (USER_STACK_TOP_VIRT - 0x1000) //  0xBFFFE000

int user_stack_init(pcb_t *proc);

void syscall_interrupt_handler(uint32_t idt_index, regs_t *regs);
