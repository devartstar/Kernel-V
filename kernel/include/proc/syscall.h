#pragma once

#include "arch/x86/interrupt.h"
#include <stdint.h>

enum {
    SYS_EXIT = 1,
    SYS_WRITE = 2,
    SYS_GETPID = 3
    /* Add more syscall entries as needed */
};

/* Define a type pointer to a function which take 6 input args of uint32_t and
 * returns type int32_t */
typedef int32_t (*syscall_handler_t)(uint32_t, uint32_t, uint32_t, uint32_t,
                                     uint32_t, uint32_t);

#define NUM_SYSCALLS 8
#define ENOSYS -38

/* An array of function pointers - different syscall handlers for different
 * index */
extern syscall_handler_t syscall_table[NUM_SYSCALLS];

void syscall_table_init(void);

void syscall_interrupt_handler(uint32_t idt_index, regs_t *regs);
