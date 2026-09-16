#ifndef LIB_STRING_H
#define LIB_STRING_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define ALIGN_UP(x, align) (((x) + ((align)-1)) & ~((align)-1))

void *memset(void *s, int c, size_t n);
char *strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);

/**
 * find the last occurence of a character and return pointer to it
 *
 * @s - string to search in
 * @c - character to search for
 *
 * @return *char - pointer to the lasr occurence or pointer to the string s if
 * char c doesnt exists
 */
char *strrchr(const char *s, int c);
void *memcpy(void *dst, const void *src, size_t n);
void strappend(char *dest, char *src);

/**
 * my_vsnprintf - utility that actuall formats the string using the string
 * formatter fmt and arguments passed.
 * @buf - string buffer to store the formatted string
 * @size - maximum size of string buffer
 * @fmt - format specifier string
 * @args - values to insert in format specifier to generate formatted string.
 *
 * @return - length of the formatted string.
 */
int my_vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

/**
 * my_snprint - strinf formatter based on the fmt and the arguments passed using
 * my_vsnprintf.
 * @buf - string buffer to store the formatted string
 * @size - maximum size of string buffer
 * @fmt - format specifier string
 * @args - values to insert in format specifier to generate formatted string.
 *
 * @return - length of the formatted string.
 */
int my_snprintf(char *buf, size_t size, const char *fmt, ...);
#endif /* LIB_STRING_H */
