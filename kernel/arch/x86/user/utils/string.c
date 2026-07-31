/*
 * string.c - Freestanding string / formatting utilities for user-mode programs.
 *
 * User programs are built as standalone flat binaries (see user.ld) and are
 * NOT linked against the kernel's lib/string.c. They also compile with
 * USER_CFLAGS, which has no kernel include path. This translation unit
 * therefore provides self-contained copies of the helpers so user code can
 * work with strings without any kernel dependency. It is linked into every C
 * user program alongside utils/syscalls.c; declarations live in
 * utils/user_utils.h.
 */
#include "user_utils.h"

uint32_t ustrlen(const char *s) {
    uint32_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

int umemeq(const void *a, const void *b, uint32_t n) {
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    for (uint32_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) {
            return 0;
        }
    }
    return 1;
}

int uvsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
    if (size == 0)
        return 0;

    char *p = buf;
    char *end = buf + size - 1;

    while (*fmt && p < end) {
        if (*fmt == '%') {
            fmt++;

            /* Padding */
            int pad_width = 0;
            char pad_char = ' ';
            if (*fmt == '0') {
                pad_char = '0';
                fmt++;
            }
            while (*fmt >= '0' && *fmt <= '9') {
                pad_width = pad_width * 10 + (*fmt - '0');
                fmt++;
            }

            /* Length modifier (only 'l' supported) */
            int long_flag = 0;
            if (*fmt == 'l') {
                fmt++;
                if (*fmt == 'l') {
                    /* long long not supported; consume and treat as long */
                    fmt++;
                }
                long_flag = 1;
            }

            switch (*fmt) {
            case 's': {
                const char *str = va_arg(args, const char *);
                int str_len = 0;

                if (!str)
                    str = "(null)";

                const char *s = str;
                while (*s++)
                    str_len++;

                int to_pad_width = (str_len > pad_width) ? 0 : pad_width - str_len;
                while (to_pad_width-- > 0 && p < end)
                    *p++ = pad_char;

                while (*str && p < end)
                    *p++ = *str++;
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                int to_pad_width = pad_width - 1;
                if (to_pad_width < 0)
                    to_pad_width = 0;
                while (to_pad_width-- > 0 && p < end)
                    *p++ = pad_char;
                if (p < end)
                    *p++ = c;
                break;
            }
            case 'd': {
                char tmp[24];
                int tmplen = 0;

                if (long_flag) {
                    long num = va_arg(args, long);
                    int is_negative = (num < 0);
                    unsigned long val =
                        is_negative ? -(unsigned long)num : (unsigned long)num;
                    if (val == 0) {
                        tmp[tmplen++] = '0';
                    } else {
                        do {
                            tmp[tmplen++] = '0' + (val % 10);
                            val /= 10;
                        } while (val && tmplen < (int)sizeof(tmp));
                    }
                    if (is_negative && tmplen < (int)sizeof(tmp))
                        tmp[tmplen++] = '-';
                } else {
                    int num = va_arg(args, int);
                    int is_negative = (num < 0);
                    unsigned int val =
                        is_negative ? -(unsigned int)num : (unsigned int)num;
                    if (val == 0) {
                        tmp[tmplen++] = '0';
                    } else {
                        do {
                            tmp[tmplen++] = '0' + (val % 10);
                            val /= 10;
                        } while (val && tmplen < (int)sizeof(tmp));
                    }
                    if (is_negative && tmplen < (int)sizeof(tmp))
                        tmp[tmplen++] = '-';
                }

                int to_pad_width = pad_width - tmplen;
                if (to_pad_width < 0)
                    to_pad_width = 0;
                while (to_pad_width-- > 0 && p < end)
                    *p++ = pad_char;

                for (int pos = tmplen - 1; pos >= 0 && p < end; pos--)
                    *p++ = tmp[pos];
                break;
            }
            case 'u': {
                char tmp[24];
                int tmplen = 0;

                if (long_flag) {
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
                    unsigned int num = va_arg(args, unsigned int);
                    if (num == 0) {
                        tmp[tmplen++] = '0';
                    } else {
                        do {
                            tmp[tmplen++] = '0' + (num % 10);
                            num /= 10;
                        } while (num && tmplen < (int)sizeof(tmp));
                    }
                }

                int to_pad_width = pad_width - tmplen;
                if (to_pad_width < 0)
                    to_pad_width = 0;
                while (to_pad_width-- > 0 && p < end)
                    *p++ = pad_char;

                for (int pos = tmplen - 1; pos >= 0 && p < end; pos--)
                    *p++ = tmp[pos];
                break;
            }
            case 'x': {
                char tmp[24];
                int tmplen = 0;

                if (long_flag) {
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

                int to_pad_width = pad_width - tmplen;
                if (to_pad_width < 0)
                    to_pad_width = 0;
                while (to_pad_width-- > 0 && p < end)
                    *p++ = pad_char;

                for (int pos = tmplen - 1; pos >= 0 && p < end; pos--)
                    *p++ = tmp[pos];
                break;
            }
            case 'p': {
                void *ptr = va_arg(args, void *);
                if (p < end - 1) {
                    *p++ = '0';
                    *p++ = 'x';
                }

                uintptr_t num = (uintptr_t)ptr;
                char tmp[20];
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

                for (int pos = tmplen - 1; pos >= 0 && p < end; pos--)
                    *p++ = tmp[pos];
                break;
            }
            case '%': {
                if (p < end)
                    *p++ = '%';
                break;
            }
            default:
                if (p < end)
                    *p++ = *fmt;
            }
        } else {
            if (p < end)
                *p++ = *fmt;
        }
        fmt++;
    }

    *p = '\0';
    return (int)(p - buf);
}

int usnprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = uvsnprintf(buf, size, fmt, args);
    va_end(args);
    return ret;
}

