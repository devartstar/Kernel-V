#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

// Core kernel includes
#include "printk.h"
#include "drivers/vga.h"
#include "panik.h"
#include "memory_map.h"
#include "pmm.h"
#include "paging.h"
#include "idt.h"
#include "arch/x86/tss.h"

#ifdef KERNEL_TESTS
#include "tests/test_printk.h"
#include "tests/test_panik.h"
#endif

// Kernel version information
#define KERNEL_NAME     "Kernel-V"
#define KERNEL_VERSION  "0.2"
#define KERNEL_AUTHOR   "Devjit"

// Common macros
#ifndef NULL
#define NULL            ((void*)0)  
#endif

#define ARRAY_SIZE(x)   (sizeof(x) / sizeof((x)[0]))

// Kernel main function (called from assembly)
void kernel_main(void);

void grow_stack(int depth);

#endif /* KERNEL_H */
