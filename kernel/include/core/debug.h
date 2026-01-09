#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H

#include "kconfig.h"
#include "lib/printk.h"

/* If Debug build: Enable debugging */
#ifdef CONFIG_BUILD_DEBUG
#define DEBUG_ENABLED 1
#else
#define DEBUG_ENABLED 0
#endif

#ifdef CONFIG_DEBUG_IDT_GDT
#define DEBUG_IDT_GDT CONFIG_DEBUG_IDT_GDT
#else
#define DEBUG_IDT_GDT 0
#endif

#ifdef CONFIG_DEBUG_TSS
#define DEBUG_TSS CONFIG_DEBUG_TSS
#else
#define DEBUG_TSS 0
#endif

#ifdef CONFIG_DEBUG_MEMORY
#define DEBUG_MEMORY CONFIG_DEBUG_MEMORY
#else
#define DEBUG_MEMORY 0
#endif

#ifdef CONFIG_DEBUG_PAGING
#define DEBUG_PAGING CONFIG_DEBUG_PAGING
#else
#define DEBUG_PAGING 0
#endif

#ifdef CONFIG_DEBUG_STACK_HEAP
#define DEBUG_STACK_HEAP CONFIG_DEBUG_STACK_HEAP
#else
#define DEBUG_STACK_HEAP 0
#endif

#ifdef CONFIG_DEBUG_PROCESS_MGMT
#define DEBUG_PROCESS_MGMT CONFIG_DEBUG_PROCESS_MGMT
#else
#define DEBUG_PROCESS_MGMT 0
#endif

//  Debug macros
#define debug_print(fmt, ...)                                                  \
	do                                                                         \
	{                                                                          \
		if (DEBUG_ENABLED)                                                     \
		{                                                                      \
			pr_verbose(fmt, ##__VA_ARGS__);                                    \
		}                                                                      \
	} while (0)

#define debug_module(sub_sys, fmt, ...)                                        \
	do                                                                         \
	{                                                                          \
		if (DEBUG_ENABLED && (DEBUG_##sub_sys))                                \
		{                                                                      \
			pr_verbose(fmt, ##__VA_ARGS__);                                    \
		}                                                                      \
	} while (0)

#endif /* KERNEL_DEBUG_H */