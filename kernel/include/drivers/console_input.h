#ifndef CONSOLE_INPUT_H
#define CONSOLE_INPUT_H

#include <stdint.h>

#define CONSOLE_INPUT_BUF_SIZE 128

void console_input_init(void);

uint32_t console_input_available(void);

int console_input_push(char in_c);

int console_input_pop(char *out_c);

uint32_t console_input_read(char *buf, uint32_t len);

#endif /* CONSOLE_INPUT_H */
