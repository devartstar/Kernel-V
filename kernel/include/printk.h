#ifndef KERNEL_PRINTK_H
#define KERNEL_PRINTK_H

#include <stdarg.h>
#include <stddef.h>

/**
 * Kernel print function - similar to printf but for kernel space
 * Supports basic format specifiers: %s, %c, %d, %x, %p
 */

// Maximum buffer size for each printk call output
#define LOG_BUF_SIZE 1024

// Log levels
#define KERN_EMERG      0   // Emergency messages
#define KERN_ALERT      1   // Alert messages  
#define KERN_CRIT       2   // Critical messages
#define KERN_ERR        3   // Error messages
#define KERN_WARNING    4   // Warning messages
#define KERN_NOTICE     5   // Notice messages
#define KERN_INFO       6   // Informational messages
#define KERN_DEBUG      7   // Debug messages

// Main printk function
int printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Convenience macros for different log levels
#define pr_emerg(fmt, ...)    printk(KERN_EMERG fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...)    printk(KERN_ALERT fmt, ##__VA_ARGS__)
#define pr_crit(fmt, ...)     printk(KERN_CRIT fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)      printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warning(fmt, ...)  printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_warn pr_warning
#define pr_notice(fmt, ...)   printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)     printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)    printk(KERN_DEBUG fmt, ##__VA_ARGS__)

// Initialize printk subsystem
void printk_init(void);

#endif /* KERNEL_PRINTK_H */
