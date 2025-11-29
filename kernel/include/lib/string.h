#ifndef LIB_STRING_H
#define LIB_STRING_H

#include <stddef.h>

void* memset(void* s, int c, size_t n);
char* strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);

#endif /* LIB_STRING_H */
