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
    port->session = sess;

    return TTY_CHAN_OK;
}

void tty_input_step(tty_session_t *sess, uint8_t byte) {
    (void)byte;

    if (!sess) {
        return;
    }

    /* Raw policy: publish immediately so the byte is readable at once */
    tty_chan_commit(&sess->input);
}

int tty_read(tty_session_t *sess, uint8_t *buf, uint32_t len) {
    int ret;

    /* check for valid session */
    if (!sess) {
        return TTY_CHAN_ERR_INVALID;
    }

    /* check if buffer is valid and read byte > 0 */
    if (!buf && len > 0) {
        return TTY_CHAN_ERR_INVALID;
    }

    if (len == 0) {
        return 0;
    }

    /* blocking read from channel */
    ret =
        tty_chan_read_blocking(&sess->input, buf, len, PROC_WAIT_CONSOLE_INPUT);
    return ret;
}

int tty_write(tty_session_t *sess, const uint8_t *buf, uint32_t len) {
    /* check for valid session */
    if (!sess) {
        return TTY_CHAN_ERR_INVALID;
    }

    if (!sess->port) {
        return TTY_CHAN_ERR_INVALID;
    }

    if (!sess->port->ops || !sess->port->ops->putc) {
        return TTY_CHAN_ERR_INVALID;
    }

    if (!buf && len > 0) {
        return TTY_CHAN_ERR_INVALID;
    }

    if (len == 0) {
        return 0;
    }

    uint32_t write_len = 0;
    for (uint32_t i = 0; i < len; i++) {
        sess->port->ops->putc(sess->port, buf[i]);
        write_len++;
    }

    return write_len;
}
