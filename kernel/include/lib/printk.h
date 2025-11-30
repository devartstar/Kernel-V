#ifndef KERNEL_PRINTK_H
#define KERNEL_PRINTK_H

#include "kconfig.h"
#include "lib/print_macros.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Kernel print function - similar to printf but for kernel space
 * Supports basic format specifiers: %s, %c, %d, %x, %p
 */

//  Maximum buffer size for each printk call output
#define LOG_BUF_SIZE 1024

//  Log levels
#define KERN_SOH "\001"			  //  Start of Header for Log Messages
#define KERN_EMERG KERN_SOH "0"	  //  Emergency messages
#define KERN_ERROR KERN_SOH "1"	  //  Error messages
#define KERN_WARN KERN_SOH "2"	  //  Warning messages
#define KERN_INFO KERN_SOH "3"	  //  Informational messages
#define KERN_VERBOSE KERN_SOH "4" //  Verbose messages

#ifndef CONFIG_TRACE_LEVEL

#define CONFIG_TRACE_LEVEL 3
#endif

//  Log level structure definition
struct loglevel
{
	char level_char;
	const char* name;
	uint8_t color;
};

/* External declaration of log levels array */
extern const struct loglevel loglevels[];
extern const int num_loglevels;

/**
 * printk - Main printk function.
 * @fmt - format string to be printed.
 * @returns number of characters printed.
 */
int printk(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

/* Convenience macros for different log levels */
#define pr_emerg(fmt, ...) printk(KERN_EMERG fmt, ##__VA_ARGS__)
#define pr_error(fmt, ...) printk(KERN_ERROR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...) printk(KERN_WARN fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...) printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_verbose(fmt, ...) printk(KERN_VERBOSE fmt, ##__VA_ARGS__)

/**
 * printk_init - Initialize printk subsystem.
 * @returns void.
 */
void printk_init(void);

/**
 * my_vsnprintf - Internal formatting function to generate the final string
 * after parsing arguments.
 * @buf - buffer to write formatted string
 * @size - size of the buffer
 * @fmt - format string
 * @args - variable argument list
 * @returns number of characters written
 */
int my_vsnprintf(char* buf, size_t size, const char* fmt, va_list args);

/**
 * ringbuf_write - Write a string to the ring buffer.
 * @str - string to write.
 * @str_len - length of the string.
 * @returns void
 */
void ringbuf_write(const char* str, size_t str_len);

#endif /* KERNEL_PRINTK_H */
