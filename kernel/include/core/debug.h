#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H

#include "lib/printk.h"

/* These flags should be set via -DDEBUG_ENABLED=1 */
#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED 0
#endif

/* These flags should be set via -DEBUG_IDT_GDT=1 */
#ifndef DEBUG_IDT_GDT
#define DEBUG_IDT_GDT 0
#endif

/* These flags should be set via -DDEBUG_TSS=1 */
#ifndef DEBUG_TSS
#define DEBUG_TSS 0
#endif

/* These flags should be set via -DDEBUG_MEMORY=1 */
#ifndef DEBUG_MEMORY
#define DEBUG_MEMORY 0
#endif

/* These flags should be set via -DDEBUG_PAGING=1 */
#ifndef DEBUG_PAGING
#define DEBUG_PAGING 0
#endif

/* These flags should be set via -DDEBUG_STACK=1 */
#ifndef DEBUG_STACK
#define DEBUG_STACK 0
#endif

// Debug macros
#define debug_print(fmt, ...) \
    do { \
        if (DEBUG_ENABLED) { \
            pr_verbose(fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define debug_module(sub_sys, fmt, ...) \
    do { \
        if (DEBUG_ENABLED && (DEBUG_##sub_sys)) { \
            pr_verbose(fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#endif /* KERNEL_DEBUG_H */