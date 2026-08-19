#include "tty_port.h"
#include "lib/printk.h"
#include "tty_chan.h"
#include "tty_session.h"

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

    /* stage it into the input buffer */
    ret = tty_chan_stage(&sess->input, byte);
    if (ret != TTY_CHAN_OK) {
        /* staging failed. drop the byte - should not block */
        if (ret == TTY_CHAN_ERR_FULL) {
            KLOG_VERBOSE("TTY", "input buffer full, dropping byte 0x%02x\n",
                         byte);
        }
        return;
    }

    /* commit the staged byte in the input buffer */
    KLOG_VERBOSE("TTY", "staged byte 0x%02x\n", byte);
    tty_input_step(sess, byte);
}
