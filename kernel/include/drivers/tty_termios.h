#ifndef TTY_TERMIOS_H
#define TTY_TERMIOS_H

#include <stdint.h>

/* c_iflag - input processing
 * it has several flags each representing 1 bit
 */
#define ICRNL (1u << 0) /* maps recieved '\r' to '\n' */

/* c_oflag - output processing
 * it has several flags each representing 1 bit
 */
#define OPOST (1u << 0) /* master switch: enable output processing */
#define ONLCR (1u << 1) /* maps '\n' to '\r\n' on output */

/* c_lflag - local / line modes */
#define ISIG (1u << 0)   /* generate signals on INTR/QUIT (future) */
#define ICANON (1u << 1) /* canonical mode (line-buffering, cooked) mode */
#define ECHO (1u << 3)   /* echo input characters to screen */

typedef struct ktermios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_lflag;
} ktermios_t;

void tty_termios_init_cooked(ktermios_t *t);

#endif /* TTY_TERMIOS_H */
