#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

// Core kernel includes
#include "printk.h"
#include "drivers/vga.h"
#include "panik.h"

#include "tests/test_panik.h"

// Kernel version information
#define KERNEL_NAME     "Kernel-V"
#define KERNEL_VERSION  "0.2"
#define KERNEL_AUTHOR   "Devjit"

// Common macros
#define NULL            ((void*)0)
#define ARRAY_SIZE(x)   (sizeof(x) / sizeof((x)[0]))

// Kernel main function (called from assembly)
void kernel_main(void);

#endif /* KERNEL_H */
