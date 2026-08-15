#ifndef TTY_PORT_H
#define TTY_PORT_H

#include <stdint.h>

typedef struct tty_port tty_port_t;
typedef struct tty_session tty_session_t;

/* Backend IO operations. A port knows to emit a raw byte */
typedef struct tty_port_ops {
    void (*putc)(tty_port_t *port, uint8_t byte);
} tty_port_ops_t;

/* port is an intermediate between a sessions(producer) and backend(consumer) */
struct tty_port {
    const tty_port_ops_t *ops;
    tty_session_t *session;
    void *private_data;
};

/**
 * tty_port_rx - hardware RX entry point. Called often from IRQ Context when one
 * byte arrives on the backing device.
 * Action: stage the byte into the sessions input channel and run the input
 * steps (commit policy)
 */
void tty_port_rx(tty_port_t *port, uint8_t byte);

#endif /* TTY_PORT_H */
