#ifndef KERNEL_H
#define KERNEL_H

#include <stddef.h>
#include <stdint.h>

//  Core kernel includes
#include "arch/x86/idt.h"
#include "arch/x86/tss.h"
#include "core/panik.h"
#include "drivers/vga.h"
#include "lib/printk.h"
#include "mm/memory_map.h"
#include "mm/paging.h"
#include "mm/pmm.h"

#ifdef KERNEL_TESTS
#include "tests/test_panik.h"
#include "tests/test_printk.h"
#endif

//  Kernel version information
#define KERNEL_NAME "Kernel-V"

#ifndef KERNEL_VERSION
#define KERNEL_VERSION "0.1.0"
#endif

#define KERNEL_AUTHOR "Devjit"

#ifndef BUILD_DATE
#define BUILD_DATE "unknown"
#endif

//  Common macros
#ifndef NULL
#define NULL ((void*)0)
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

//  Kernel main function (called from assembly)
void kernel_main(void);

void grow_stack(int depth);

#endif /* KERNEL_H */
