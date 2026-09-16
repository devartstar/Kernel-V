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
    SYS_READ = 6,
    SYS_CLOSE = 7,
    SYS_LSEEK = 8,
    /* Add more syscall entries as needed */
};

/* Define a type pointer to a function which take 6 input args of uint32_t and
 * returns type int32_t */
typedef int32_t (*syscall_handler_t)(uint32_t, uint32_t, uint32_t, uint32_t,
                                     uint32_t, uint32_t);

/* Syscalls are 1-indexed and the table is indexed directly by syscall number,
 * so the table must have one slot per number including the highest one
 * (SYS_LSEEK). Keep this as (highest syscall + 1) when adding new syscalls. */
#define NUM_SYSCALLS (SYS_LSEEK + 1)
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
