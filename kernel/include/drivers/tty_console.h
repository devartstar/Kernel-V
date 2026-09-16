#ifndef TTY_CONSOLE_H
#define TTY_CONSOLE_H

#include "drivers/tty_port.h"

/**
 * tty_console_init - initializes the tty console session.
 * inintialize the console port operation with defined putc
 * inirialize the consoe port session
 *
 * @ return the status of the console session init
 */
int tty_console_init(void);

/*
 * tty_console_rx - recieve a byte and add it to the tty port rx queue.
 * console port stages and commits the byte
 */
void tty_console_rx(uint8_t byte);

/**
 * tty_console_session - getter for the reference of the global tty session
 */
tty_session_t *tty_console_session(void);

#endif /* TTY_CONSOLE_H */
