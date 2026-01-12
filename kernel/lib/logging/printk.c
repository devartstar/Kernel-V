#include "lib/printk.h"
#include "drivers/vga.h"
#include "lib/logbackend.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* Circular log buffer for storing kernel messages */
static char log_buffer[LOG_BUF_SIZE];

/* Ring buffer pointers */
static size_t rb_head = 0; //  Position of next byte to be written
static size_t rb_tail = 0; //  Position of oldest byte in buffer

/* Log levels for kernel messages */
const struct loglevel loglevels[] = {
    {'0', "EMERG", VGA_COLOR(VGA_BLACK, VGA_RED)},
    {'1', "ERROR", VGA_COLOR(VGA_BLACK, VGA_YELLOW)},
    {'2', "WARN", VGA_COLOR(VGA_BLACK, VGA_WHITE)},
    {'3', "INFO", VGA_COLOR(VGA_BLACK, VGA_DARK_GREY)},
    {'4', "VERBOSE", VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN)}};

const int num_loglevels = sizeof(loglevels) / sizeof(loglevels[0]);

/**
 * ringbuf_putc - Append one character to the circular ring buffer
 * @ch - character to append
 * @returns void
 */
static void ringbuf_putc(char ch) {
    log_buffer[rb_head] = ch;
    rb_head = (rb_head + 1) % LOG_BUF_SIZE;

    /* If buffer is full, advance tail to drop oldest byte */
    if (rb_head == rb_tail) {
        rb_tail = (rb_tail + 1) % LOG_BUF_SIZE;
    }
}

void ringbuf_write(const char *str, size_t str_len) {
    for (size_t i = 0; i < str_len; i++) {
        ringbuf_putc(str[i]);
    }
}

/**
 * find_loglevel - Find log level by character.
 * @level_char - character representing log level.
 * @returns index of log level in loglevels array or -1 if not found.
 */
static int find_loglevel(char level_char) {
    for (int i = 0; i < num_loglevels; i++) {
        if (loglevels[i].level_char == level_char) {
            return i;
        }
    }
    return -1;
}

void printk_init(void) {
    rb_head = 0;
    rb_tail = 0;
    vga_init();

    num_logs_backend = 0;
    register_log_backend(vga_backend);
    register_log_backend(ringbuf_backend);
}

/* TODO: Modify this buffer generation as desired */
int my_vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
    char *p = buf;
    char *end = buf + size - 1;

    while (*fmt && p < end) {
        if (*fmt == '%') {
            fmt++;

            //  Handle Padding
            int pad_width = 0;
            char pad_char = ' ';
            //  Handle zero padding
            if (*fmt == '0') {
                pad_char = '0';
                fmt++;
            }
            //  Handle Padding width
            while (*fmt >= '0' && *fmt <= '9') {
                pad_width = pad_width * 10 + (*fmt - '0');
                fmt++;
            }

            // Handle length modifier (only 'l' supported for now)
            int long_flag = 0;
            int longlong_flag = 0;
            if (*fmt == 'l') {
                fmt++;
                if (*fmt == 'l') {
                    longlong_flag = 1;
                    fmt++;
                } else {
                    long_flag = 1;
                }
            }

            switch (*fmt) {
            case 's': {
                const char *str = va_arg(args, const char *);
                int str_len = 0;

                //  Handle null strings
                if (!str)
                    str = "(null)";

                //  Calculate string length
                const char *s = str;
                while (*s++)
                    str_len++;

                //  claculate the padding needed
                int to_pad_width;
                if (str_len > pad_width) {
                    to_pad_width = 0;
                } else {
                    to_pad_width = pad_width - str_len;
                }

                //  Write padding if needed
                while (to_pad_width-- > 0 && p < end) {
                    *p++ = pad_char;
                }

                //  Write the string
                while (*str && p < end) {
                    *p++ = *str++;
                }
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);

                //  calculate the padding needed
                int to_pad_width = pad_width - 1;
                if (to_pad_width < 0)
                    to_pad_width = 0;

                //  Write padding if needed
                while (to_pad_width-- > 0 && p < end) {
                    *p++ = pad_char;
                }

                //  Write the character
                if (p < end)
                    *p++ = c;
                break;
            }
            case 'd': {
                char tmp[24];
                int tmplen = 0;

                if (longlong_flag) {
                    /* long long is not supported
                    long long num = va_arg(args, long long);
                    //  convert signed decimal into char array
                    int is_negative = (num < 0);
                    unsigned long long val = is_negative ? -num : num;
                    if (val == 0)
                    {
                        tmp[tmplen++] = '0';
                    }
                    else
                    {
                        do
                        {
                            tmp[tmplen++] = '0' + (val % 10);
                            val /= 10;
                        } while (val && tmplen < (int)sizeof(tmp));
                    }

                    if (is_negative && tmplen < (int)sizeof(tmp))
                    {
                        tmp[tmplen++] = '-'; //  Fixed: increment tmplen
                    }
                    */
                } else if (long_flag) {
                    long num = va_arg(args, long);
                    //  convert signed decimal into char array
                    int is_negative = (num < 0);
                    unsigned long val = is_negative ? -num : num;
                    if (val == 0) {
                        tmp[tmplen++] = '0';
                    } else {
                        do {
                            tmp[tmplen++] = '0' + (val % 10);
                            val /= 10;
                        } while (val && tmplen < (int)sizeof(tmp));
                    }

                    if (is_negative && tmplen < (int)sizeof(tmp)) {
                        tmp[tmplen++] = '-'; //  Fixed: increment tmplen
                    }
                } else {
                    int num = va_arg(args, int);
                    int is_negative = (num < 0);
                    unsigned int val = is_negative ? -num : num;
                    if (val == 0) {
                        tmp[tmplen++] = '0';
                    } else {
                        do {
                            tmp[tmplen++] = '0' + (val % 10);
                            val /= 10;
                        } while (val && tmplen < (int)sizeof(tmp));
                    }

                    if (is_negative && tmplen < (int)sizeof(tmp)) {
                        tmp[tmplen++] = '-'; //  Fixed: increment tmplen
                    }
                }

                //  calculate the padding needed
                int to_pad_width = pad_width - tmplen;
                if (to_pad_width < 0)
                    to_pad_width = 0;

                //  Write padding if needed
                while (to_pad_width-- > 0 && p < end) {
                    *p++ = pad_char;
                }

                //  reverse it and store in output
                for (int pos = tmplen - 1; pos >= 0 && p < end;
                     pos--) //  Fixed: start from tmplen-1
                {
                    *p++ = tmp[pos];
                }
                break;
            }
            case 'u': {
                char tmp[24];
                int tmplen = 0;

                // Convert number value to a character array
                if (longlong_flag) {
                    /* long long not supported yet
                    unsigned long long num = va_arg(args, unsigned long long);
                    if (num == 0)
                    {
                        tmp[tmplen++] = '0';
                    }
                    else
                    {
                        do
                        {
                            tmp[tmplen++] = '0' + (num - num / 10 * 10);
                            num /= 10;
                        } while (num && tmplen < (int)sizeof(tmp));
                    }
                    */
                } else if (long_flag) {
                    unsigned long num = va_arg(args, unsigned long);
                    if (num == 0) {
                        tmp[tmplen++] = '0';
                    } else {
                        do {
                            tmp[tmplen++] = '0' + (num % 10);
                            num /= 10;
                        } while (num && tmplen < (int)sizeof(tmp));
                    }
                } else {
                    int num = va_arg(args, unsigned int);
                    if (num == 0) {
                        tmp[tmplen++] = '0';
                    } else {
                        do {
                            tmp[tmplen++] = '0' + (num % 10);
                            num /= 10;
                        } while (num && tmplen < (int)sizeof(tmp));
                    }
                }
                //  calculate the padding needed
                int to_pad_width = pad_width - tmplen;
                if (to_pad_width < 0) {
                    to_pad_width = 0;
                }

                //  Write padding if needed
                while (to_pad_width-- > 0 && p < end) {
                    *p++ = pad_char;
                }

                //  reverse it and store in output
                for (int pos = tmplen - 1; pos >= 0 && p < end;
                     pos--) //  Fixed: start from tmplen-1
                {
                    *p++ = tmp[pos];
                }
                break;
            }
            case 'x': {
                char tmp[24];
                int tmplen = 0;

                //  convert unsigned hex into char array
                if (longlong_flag) {
                    /* long long not supported yet
                    unsigned long long num = va_arg(args, unsigned long long);
                    if (num == 0)
                    {
                        tmp[tmplen++] = '0';
                    }
                    else
                    {
                        do
                        {
                            int digit = num % 16;
                            tmp[tmplen++] =
                                (digit < 10) ? ('0' + digit) : ('a' + digit -
                    10); num /= 16; } while (num && tmplen < (int)sizeof(tmp));

                    }
                    */
                } else if (long_flag) {
                    unsigned long num = va_arg(args, unsigned long);
                    if (num == 0) {
                        tmp[tmplen++] = '0';
                    } else {
                        do {
                            int digit = num % 16;
                            tmp[tmplen++] = (digit < 10) ? ('0' + digit)
                                                         : ('a' + digit - 10);
                            num /= 16;
                        } while (num && tmplen < (int)sizeof(tmp));
                    }
                } else {
                    unsigned int num = va_arg(args, unsigned int);
                    if (num == 0) {
                        tmp[tmplen++] = '0';
                    } else {
                        do {
                            int digit = num % 16;
                            tmp[tmplen++] = (digit < 10) ? ('0' + digit)
                                                         : ('a' + digit - 10);
                            num /= 16;
                        } while (num && tmplen < (int)sizeof(tmp));
                    }
                }

                //  calculate the padding needed
                int to_pad_width = pad_width - tmplen;
                if (to_pad_width < 0)
                    to_pad_width = 0;

                //  Write padding if needed
                while (to_pad_width-- > 0 && p < end) {
                    *p++ = pad_char;
                }

                //  reverse it and store in output
                for (int pos = tmplen - 1; pos >= 0 && p < end; pos--) {
                    *p++ = tmp[pos];
                }
                break;
            }
            case 'p': {
                void *ptr = va_arg(args, void *);
                //  Add "0x" prefix
                if (p < end - 1) {
                    *p++ = '0';
                    *p++ = 'x';
                }

                //  convert pointer to hex
                uintptr_t num = (uintptr_t)ptr;
                char tmp[20]; //  Enough for 64-bit pointer
                int tmplen = 0;

                if (num == 0) {
                    tmp[tmplen++] = '0';
                } else {
                    do {
                        int digit = num % 16;
                        tmp[tmplen++] =
                            (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
                        num /= 16;
                    } while (num && tmplen < (int)sizeof(tmp));
                }

                //  reverse it and store in output
                for (int pos = tmplen - 1; pos >= 0 && p < end; pos--) {
                    *p++ = tmp[pos];
                }
                break;
            }
            case '%': {
                if (p < end)
                    *p++ = '%';
                break;
            }
            default:
                if (p < end) {
                    *p++ = *fmt; //  Just copy unknown specifier
                }
            }
        } else {
            if (p < end)
                *p++ = *fmt; //  Copy normal character
        }
        fmt++;
    }
    *p = '\0';
    return p - buf;
}

/**
 * vprintk - Internal printk function handling arguments va_list.
 * @fmt - format string to be printed.
 * @args - variable argument list.
 * @returns number of characters printed.
 */
int vprintk(const char *fmt, va_list args) {
    char tmp[LOG_BUF_SIZE];

    int len = my_vsnprintf(tmp, sizeof(tmp), fmt, args);

    log_dispatch_to_backends(tmp, len, WHITE_ON_BLACK);

    return len;
}

int printk(const char *fmt, ...) {
    char level_prefix[32];
    const char *actual_fmt = fmt;
    int log_level_idx = -1;
    int total_len = 0;

    if (fmt[0] == '\001' && fmt[1] >= '0' && fmt[1] <= '7') {
        int idx = find_loglevel(fmt[1]);
        if (idx >= 0) {
            log_level_idx = idx;
        }
        actual_fmt = fmt + 2;
    }

    if (log_level_idx != -1 && log_level_idx > CONFIG_TRACE_LEVEL) {
        return 0;
    }

    if (log_level_idx != -1) {
        //  Build the level prefix: [LEVEL]
        char *p = level_prefix;
        *p++ = '[';

        //  Copy the correct log level name
        const char *name = loglevels[log_level_idx].name;
        while (*name) {
            *p++ = *name++;
        }

        *p++ = ']';
        *p++ = ' ';
        *p = '\0';

        int prefix_len = p - level_prefix;

        //  Write to ring buffer and console with colored prefix
        log_dispatch_to_backends(level_prefix, prefix_len,
                                 loglevels[log_level_idx].color);
        total_len += prefix_len;
    }

    va_list args;
    va_start(args, fmt);
    int msg_len = vprintk(actual_fmt, args);
    va_end(args);

    total_len += msg_len;
    return total_len;
}
