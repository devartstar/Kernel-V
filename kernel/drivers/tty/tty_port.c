#include "tty_port.h"
#include "lib/printk.h"
#include "tty_chan.h"
#include "tty_session.h"

static void tty_input_chan_sink(void *ctx, uint8_t byte);

void tty_port_sink(void *ctx, uint8_t byte) {
    tty_port_t *port = (tty_port_t *)ctx;
    port->ops->putc(port, byte);
}

void tty_port_rx(tty_port_t *port, uint8_t byte) {
    tty_session_t *sess;
    int ret;

    /* check for valid port */
    if (!port) {
        return;
    }

    /* get the session from the port */
    sess = port->session;
    if (!sess) {
        /* port is not yet onboarded, drop the byte */
        return;
    }

    /* invoke the session's input pipeline */
    if (sess->in_pipeline) {
        tty_pipeline_run(sess->in_pipeline, byte, tty_input_chan_sink, sess);
    } else {
        /* no imput pipeline scenario line discipline stores directly in buffer
         */
        tty_input_chan_sink(sess, byte);
    }
}

static void tty_input_chan_sink(void *ctx, uint8_t byte) {
    int ret;

    tty_session_t *session = (tty_session_t *)ctx;

    /* stage the byte; when input buffer full, drop it */
    ret = tty_chan_stage(&session->input, byte);
    if (ret != TTY_CHAN_OK) {
        /* NOTE: this is called under serial IRQ ctx, must not block it on
         * buffer full or any error */
        KLOG_VERBOSE("TTY", "input buffer full, dropping byte 0x%02x\n", byte);
        return;
    }

    /* commit the staged byte in input buffer */
    tty_input_step(session, byte);
}
