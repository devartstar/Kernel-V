#include "tty_session.h"
#include "lib/printk.h"
#include "tty_port.h"

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
    KLOG_VERBOSE("TTY", "committed byte 0x%02x\n", byte);
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

    KLOG_VERBOSE("TTY", "read returned %d byte(s)\n", ret);
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

    /* write_len is the number of bytes written to port before processing
     * number of actual bytes might differ based on line processing. */
    uint32_t write_len = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (sess->out_pipeline) {
            tty_pipeline_run(sess->out_pipeline, buf[i], tty_port_sink,
                             sess->port);
        } else {
            tty_port_sink(sess->port, buf[i]);
        }
        write_len++;
    }

    KLOG_VERBOSE("TTY", "wrote %u byte(s)\n", write_len);
    return write_len;
}
