#ifndef TTY_SESSION_H
#define TTY_SESSION_H

#include "tty_chan.h"
#include "tty_pipeline.h"
#include "tty_port.h"
#include "tty_termios.h"
#include <stdint.h>

#define TTY_INPUT_BUF_SIZE 256
#define TTY_OUTPUT_BUF_SIZE 256

struct tty_session {
    tty_chan_t input;  /* hardware -> process */
    tty_chan_t output; /* process -> hardware */
    tty_port_t *port;  /* backend driven by this session */

    uint8_t in_buf[TTY_INPUT_BUF_SIZE];
    uint8_t out_buf[TTY_OUTPUT_BUF_SIZE];

    /* single source of for current tty session */
    ktermios_t term;

    /* refernecing a pointer to pipeline object */
    tty_pipeline_t *out_pipeline;
    tty_pipeline_t *in_pipeline;
};

/**
 * tty_session_init - initializes a tty session - both the input and output
 * channel and bind the session with port.
 * @sess - session to init
 * @port - backend port to attach to the session.
 *
 * @returns TTY_CHAN_OK or TTY_CHAN_ERR_INVALID
 */
int tty_session_init(tty_session_t *sess, tty_port_t *port);

/**
 * tty_input_step - commit policy hook.
 * Phase 2 commit immediately. todo: update later.
 * Called by tty_port_rx after a byte is staged.
 */
void tty_input_step(tty_session_t *sess, uint8_t byte);

/**
 * tty_read - process-facing blocking read from the input channel.
 * tty_write - process-facing write to the output channel (emits to port).
 */
int tty_read(tty_session_t *sess, uint8_t *buf, uint32_t len);
int tty_write(tty_session_t *sess, const uint8_t *buf, uint32_t len);

/**
 * API to get and set the terminal session states
 */
void tty_session_get_termios(tty_session_t *s, ktermios_t *out);
int tty_session_set_termios(tty_session_t *s, const ktermios_t *in);

#endif /* TTY_SESSION_H */
