#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H

#include "lib/printk.h"

// Debug build configuration
#ifdef KERNEL_DEBUG
    #define DEBUG_ENABLED 1
    #define DEBUG_VERBOSE 1
    #define DEBUG_IDT_GDT 1
    #define DEBUG_MEMORY 1
    #define DEBUG_PAGING 1
    #define DEBUG_STACK 1
    #define DEBUG_TSS 1
#else
    #define DEBUG_ENABLED 0
    #define DEBUG_VERBOSE 0
    #define DEBUG_IDT_GDT 0
    #define DEBUG_MEMORY 0
    #define DEBUG_PAGING 0
    #define DEBUG_STACK 0
    #define DEBUG_TSS 0
#endif

// Debug macros
#define debug_print(fmt, ...) \
    do { \
        if (DEBUG_ENABLED) { \
            printk("[DEBUG] " fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define debug_verbose(fmt, ...) \
    do { \
        if (DEBUG_VERBOSE) { \
            printk("[VERBOSE] " fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define debug_info(subsys, fmt, ...) \
    do { \
        if (DEBUG_##subsys) { \
            printk("[DEBUG:%s] " fmt, #subsys, ##__VA_ARGS__); \
        } \
    } while(0)

#endif /* KERNEL_DEBUG_H */