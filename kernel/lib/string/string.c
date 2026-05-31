#include "lib/string.h"
#include <stddef.h>

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c)
            last = s;
        s++;
    }
    return (char *)(c == 0 ? s : last);
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void strappend(char *dest, char *src) {
    while (*dest)
        dest++;
    while ((*dest++ = *src++))
        ;
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

int my_snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = my_vsnprintf(buf, size, fmt, args);
    va_end(args);
    return ret;
}
