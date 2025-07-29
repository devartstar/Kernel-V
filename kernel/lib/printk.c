#include "printk.h"
#include "drivers/vga.h"
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

// Circular log buffer for storing kernel messages
static char log_buffer[LOG_BUF_SIZE];

// Ring buffer pointers
static size_t rb_head = 0;  // Position of next byte to be written
static size_t rb_tail = 0;  // Position of oldest byte in buffer

/**
 * Append one character to the circular ring buffer
 */
static void ringbuf_putc(char ch) {
    log_buffer[rb_head] = ch;
    rb_head = (rb_head + 1) % LOG_BUF_SIZE;
    
    // If buffer is full, advance tail to drop oldest byte
    if (rb_head == rb_tail) {
        rb_tail = (rb_tail + 1) % LOG_BUF_SIZE;
    }
}

/**
 * Write a string to the ring buffer
 */
static void ringbuf_write(const char* str, size_t str_len) {
    for (size_t i = 0; i < str_len; i++) {
        ringbuf_putc(str[i]);
    }
}

/**
 * Initialize printk subsystem
 */
void printk_init(void) {
    rb_head = 0;
    rb_tail = 0;
    vga_init();
}

/**
 * My Implementation of vsnprintf 
 * snprintf (buf, size to write, template, value) writes to a buffer 
 * Supported format specifiers: %s (string), %c (char), %d (int), %x (hex), %p (pointer)
 * Return number of characters written
 * TODO: Modify this buffer generation as desired
 */
static int my_vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    char *p = buf;
    char *end = buf + size - 1;

    while(*fmt && p < end)
    {
        if(*fmt == '%') {
            fmt++;
            switch(*fmt) {
                case 's': {
                    const char *str = va_arg(args, const char*);
                    while(*str && p < end) {
                        *p++ = *str++;
                    }
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    if(p < end) *p++ = c;
                    break;
                }
                case 'd': {
                    int num = va_arg(args, int);
                    // convert signed decimal into char array
                    int is_negative = (num < 0);
                    unsigned int val = is_negative ? -num : num;
                    char tmp[12];
                    int tmplen = 0;
                    
                    do {
                        tmp[tmplen++] = '0' + (val % 10);
                        val /= 10;
                    } while(val && tmplen < (int)sizeof(tmp));

                    if(is_negative && tmplen < (int)sizeof(tmp))
                    {
                        tmp[tmplen++] = '-';  // Fixed: increment tmplen
                    }

                    // reverse it and store in output
                    for(int pos = tmplen - 1; pos >= 0 && p < end; pos--)  // Fixed: start from tmplen-1
                    {
                        *p++ = tmp[pos];
                    }
                    break;
                }
                case 'x': {
                    unsigned int num = va_arg(args, unsigned int);
                    // convert unsigned hex into char array
                    char tmp[12];
                    int tmplen = 0;
                    
                    if (num == 0) {
                        tmp[tmplen++] = '0';
                    } else {
                        do {
                            int digit = num % 16;
                            tmp[tmplen++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
                            num /= 16;
                        } while(num && tmplen < (int)sizeof(tmp));
                    }

                    // reverse it and store in output
                    for(int pos = tmplen - 1; pos >= 0 && p < end; pos--)
                    {
                        *p++ = tmp[pos];
                    }
                    break;
                }
                case 'p': {
                    void *ptr = va_arg(args, void*);
                    // Add "0x" prefix
                    if (p < end - 1) {
                        *p++ = '0';
                        *p++ = 'x';
                    }
                    
                    // convert pointer to hex
                    uintptr_t num = (uintptr_t)ptr;
                    char tmp[20];  // Enough for 64-bit pointer
                    int tmplen = 0;
                    
                    if (num == 0) {
                        tmp[tmplen++] = '0';
                    } else {
                        do {
                            int digit = num % 16;
                            tmp[tmplen++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
                            num /= 16;
                        } while(num && tmplen < (int)sizeof(tmp));
                    }

                    // reverse it and store in output
                    for(int pos = tmplen - 1; pos >= 0 && p < end; pos--)
                    {
                        *p++ = tmp[pos];
                    }
                    break;
                }
                case '%': {
                    if(p < end) *p++ = '%';
                    break;
                }
                default:
                    if(p < end) *p++ = *fmt;  // Just copy unknown specifier
            }
        } else {
            if(p < end) *p++ = *fmt;  // Copy normal character
        }
        fmt++;
    }
    *p = '\0';
    return p - buf;  
}

int printk(const char *fmt, ...)
{
    char tmp[LOG_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    int len = my_vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    // Write to ring buffer
    ringbuf_write(tmp, len);

    // Write to console using VGA driver (handles newlines properly)
    vga_print_string(tmp, WHITE_ON_BLACK);

    return len;
}
