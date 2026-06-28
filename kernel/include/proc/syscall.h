#pragma once

#include "arch/x86/interrupt.h"
#include <stdint.h>

/* syscall io buffer size */
#define SYSCALL_IO_BUFSZ 256

enum {
    SYS_EXIT = 1,
    SYS_WRITE = 2,
    SYS_GETPID = 3,
    SYS_SCHED_YIELD = 4,
    SYS_OPEN = 5,
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

/* syscall_table_init - Initialize the syscall table */
void syscall_table_init(void);

/* syscall_interrupt_handler - Entry for syscall interrupt and calls the
 * appropriate handler for the syscall index
 * @idt_index - intereupt vector number for the syscall
 * @regs - pointer to the register values for the interrupe
 *
 * @return void
 * */
void syscall_interrupt_handler(uint32_t idt_index, regs_t *regs);
