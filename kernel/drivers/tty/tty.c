#include "drivers/tty.h"
#include "drivers/tty_port.h"
#include "drivers/tty_session.h"

/**
 * Foreground session - a single terminal that currently recieves bytes from
 * physical input source (serial UART, keyboard). One physical input, N session
 * -> this pointer is the demux selector
 */
static tty_session_t *g_active_session;

void tty_set_active(tty_session_t *sess) { g_active_session = sess; }

tty_session_t *tty_get_active(void) { return g_active_session; }

/**
 * Route one physical input byte to the foreground session's port.
 * Called from IRQ context (Serial handler). Reading a 32 bit aligned pointer is
 * atomic so no lock needed for lookup itself.
 */
void tty_input_byte(uint8_t byte) {
    tty_session_t *sess = g_active_session;
    if (!sess || !sess->port) {
        return;
    }

    tty_port_rx(sess->port, byte);
}
