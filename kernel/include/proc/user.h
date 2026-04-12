#pragma once

#include "proc/proc.h"
#include <stdint.h>

/* Range of user space address */
#define USER_VIRT_MIN 0x00400000
#define USER_VIRT_MAX 0xC0000000

/* 64Kb of user space stack */
#define USER_STACK_TOP_VIRT 0xBFFFF000
#define USER_STACK_SIZE (4 * PAGE_SIZE) // 64 KB
#define USER_STACK_BOTTOM_VIRT (USER_STACK_TOP_VIRT - USER_STACK_SIZE)

#define USER_CODE_VIRT                                                         \
    0x00400000 // Standard user code start (4MB) instead of 0x18000

int user_stack_init(pcb_t *proc);

void switch_to_usermode(uint32_t entry, uint32_t user_stack_top);

pcb_t *userproc_create_from_blob(const char *name, const uint8_t *blob_start,
                                 uint32_t blob_size);

void userproc_kernel_entry(void *args);
