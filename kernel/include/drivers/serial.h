#pragma once

#include <stddef.h>
#include <stdint.h>

void serial_init(void);
void serial_write(const char *data, size_t len);
void serial_putc(char c);
