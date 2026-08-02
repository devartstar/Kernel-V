#ifndef CONSOLE_INPUT_H
#define CONSOLE_INPUT_H

#include <stdint.h>

#define CONSOLE_INPUT_BUF_SIZE 128

/**
 * console_input_init will initialize the console buffer and other console
 * related global structures.
 */
void console_input_init(void);

/**
 * console_input_available will return the number of characters in the console
 * buffer
 *
 * @return the number of characters in the console buffer.
 */
uint32_t console_input_available(void);

/**
 * console_input_push will add a character to the console buffer
 *
 * @in_c - character to add to the conosle buffer
 *
 * @return the status code. < 0 signifies error.
 */
int console_input_push(char in_c);

/**
 * console_input_pop pops out a character to the argument passed reference.
 *
 * @out_c - reference to the output character.
 *
 * @return status of the pop. < 0 signifies error.
 */
int console_input_pop(char *out_c);

/**
 * console_input_read reads a string of length len from the console buffer
 *
 * @buf is the buffer to read into.
 * @len length of the buffer to read
 *
 * @return number of characters read
 */
int console_input_read(char *buf, uint32_t len);

#endif /* CONSOLE_INPUT_H */
