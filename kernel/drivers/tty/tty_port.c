#include "tty_port.h"
#include "tty_chan.h"
#include "tty_session.h"

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
        return;
    }

    /* commit the staged byte in the input buffer */
    tty_input_step(sess, byte);
}
