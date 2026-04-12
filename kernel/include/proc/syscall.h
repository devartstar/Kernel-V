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

/* usr_ptr_validate - check if the pointer points to address in user space
 * @ptr - address to validate
 *
 * @return 1 if valid and 0 if invalid
 */
static uint8_t usr_ptr_validate(uint32_t ptr);

/* usr_range_is_valid - check if the range of address is in user space
 * @ptr - starting address of the range
 * @len - length of the address
 *
 * @return 1 if valid and 0 if invalid
 */
static uint8_t usr_range_is_valid(uint32_t ptr, uint32_t len);
