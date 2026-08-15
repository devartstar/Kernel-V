#include "tty_session.h"

int tty_session_init(tty_session_t *sess, tty_port_t *port) {
    int err;

    /* session should be valid */
    if (!sess) {
        return TTY_CHAN_ERR_INVALID;
    }

    /* port should be vaild */
    if (!port) {
        return TTY_CHAN_ERR_INVALID;
    }

    /* Initialize the input channel */
    err = tty_chan_init(&sess->input, sess->in_buf, TTY_INPUT_BUF_SIZE);
    if (err != TTY_CHAN_OK) {
        return err;
    }

    /* Initialize the output channel */
    err = tty_chan_init(&sess->output, sess->out_buf, TTY_OUTPUT_BUF_SIZE);
    if (err != TTY_CHAN_OK) {
        return err;
    }

    /* Initialize the backing port for the channel */
    sess->port = port;

    return TTY_CHAN_OK;
}
