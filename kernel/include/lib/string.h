#ifndef LIB_STRING_H
#define LIB_STRING_H

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
#endif /* LIB_STRING_H */
