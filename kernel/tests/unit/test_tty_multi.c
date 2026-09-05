/**
 * @file test_tty_multi.c  Proof that two TTY sessions are fully independent.
 *
 * Everything in Phase 6-multi was plumbing so that N sessions can coexist with
 * zero shared mutable state. This test instantiates TWO sessions on TWO ports
 * and asserts input, output and signals never cross between them.
 */

#include "tests/test_tty_multi.h"
#include "drivers/tty.h"
#include "drivers/tty_chan.h"
#include "drivers/tty_port.h"
#include "drivers/tty_session.h"
#include "lib/printk.h"
#include "proc/signal.h"

/* A port sink that records every byte the backend would emit. */
typedef struct cap {
    uint8_t buf[64];
    uint32_t len;
} cap_t;

/* Per-session signal spy: records which session's cord fired and what sig. */
typedef struct sig_spy {
    int last_sig;
    uint32_t count;
} sig_spy_t;

static void cap_putc(tty_port_t *port, uint8_t byte) {
    cap_t *c = (cap_t *)port->private_data;
    if (c && c->len < sizeof(c->buf)) {
        c->buf[c->len++] = byte;
    }
}
static const tty_port_ops_t g_cap_port_ops = {.putc = cap_putc};

static void spy_on_signal(void *ctx, int sig) {
    sig_spy_t *spy = (sig_spy_t *)ctx;
    spy->last_sig = sig;
    spy->count++;
}

/* Two sessions + ports + capture buffers. MUST be static: a live session is
 * full of self-referential pointers and can never be copied/moved. */
static tty_session_t g_sessA, g_sessB;
static tty_port_t g_portA, g_portB;
static cap_t g_capA, g_capB;

/* (Re)build a clean pair of independent sessions. */
static uint8_t build_two_sessions(void) {
    g_capA.len = 0;
    g_capB.len = 0;

    g_portA.ops = &g_cap_port_ops;
    g_portA.session = NULL;
    g_portA.private_data = &g_capA;

    g_portB.ops = &g_cap_port_ops;
    g_portB.session = NULL;
    g_portB.private_data = &g_capB;

    if (tty_session_init(&g_sessA, &g_portA) != TTY_CHAN_OK) {
        return 0;
    }
    if (tty_session_init(&g_sessB, &g_portB) != TTY_CHAN_OK) {
        return 0;
    }
    return 1;
}

/* Feed a byte string into a session's RX port (as a driver IRQ would). */
static void feed_port(tty_port_t *port, const char *s) {
    for (const char *p = s; *p; p++) {
        tty_port_rx(port, (uint8_t)*p);
    }
}

/* INPUT isolation: bytes delivered to A must be readable on A only. */
static uint8_t test_input_isolation(void) {
    uint8_t out[64];
    int n;

    if (!build_two_sessions()) {
        KLOG_ERROR("TTY_MULTI", "input: fixture init failed.\n");
        return 0;
    }

    /* deliver a complete line to A only (newline commits it in canon mode) */
    feed_port(&g_portA, "hi\n");

    /* A must have exactly the committed line; B must have nothing */
    if (tty_chan_readable(&g_sessB.input) != 0) {
        KLOG_ERROR("TTY_MULTI",
                   "input: session B saw bytes meant for A (leak!).\n");
        return 0;
    }

    n = tty_chan_read(&g_sessA.input, out, sizeof(out));
    if (n != 3 || out[0] != 'h' || out[1] != 'i' || out[2] != '\n') {
        KLOG_ERROR("TTY_MULTI", "input: session A line wrong. got n=%d.\n", n);
        return 0;
    }

    return 1;
}

/* OUTPUT isolation: a write to A must reach A's backend only. */
static uint8_t test_output_isolation(void) {
    if (!build_two_sessions()) {
        KLOG_ERROR("TTY_MULTI", "output: fixture init failed.\n");
        return 0;
    }

    /* write distinct payloads to each session */
    tty_write(&g_sessA, (const uint8_t *)"A", 1);
    tty_write(&g_sessB, (const uint8_t *)"B", 1);

    /* each capture must hold exactly its own byte */
    if (g_capA.len != 1 || g_capA.buf[0] != 'A') {
        KLOG_ERROR("TTY_MULTI", "output: port A capture wrong. len=%u.\n",
                   g_capA.len);
        return 0;
    }
    if (g_capB.len != 1 || g_capB.buf[0] != 'B') {
        KLOG_ERROR("TTY_MULTI", "output: port B capture wrong. len=%u.\n",
                   g_capB.len);
        return 0;
    }

    return 1;
}

/* SIGNAL isolation: ^C on A's line signals A's cord only. */
static uint8_t test_signal_isolation(void) {
    sig_spy_t spyA = {.last_sig = 0, .count = 0};
    sig_spy_t spyB = {.last_sig = 0, .count = 0};

    if (!build_two_sessions()) {
        KLOG_ERROR("TTY_MULTI", "signal: fixture init failed.\n");
        return 0;
    }

    /* redirect each session's cord to its own spy (real per-session path,
     * captured instead of delivered to parked procs) */
    g_sessA.canon.on_signal = spy_on_signal;
    g_sessA.canon.signal_ctx = &spyA;
    g_sessB.canon.on_signal = spy_on_signal;
    g_sessB.canon.signal_ctx = &spyB;

    /* type Ctrl-C (0x03 = VINTR) on A's terminal */
    tty_port_rx(&g_portA, 0x03);

    /* A's cord fired exactly once with SIGINT; B never fired */
    if (spyA.count != 1 || spyA.last_sig != SIGINT) {
        KLOG_ERROR("TTY_MULTI", "signal: A cord wrong. count=%u sig=%d.\n",
                   spyA.count, spyA.last_sig);
        return 0;
    }
    if (spyB.count != 0) {
        KLOG_ERROR("TTY_MULTI",
                   "signal: B cord fired for A's ^C (leak!). count=%u.\n",
                   spyB.count);
        return 0;
    }

    return 1;
}

uint32_t tty_run_multi_session_cases(void) {
    uint32_t failed = 0;

    if (!build_two_sessions()) {
        KLOG_ERROR("TTY_MULTI", "fixture: two-session init failed.\n");
        return 1;
    }

    /* sanity: the two sessions are genuinely distinct objects */
    if (&g_sessA == &g_sessB || g_sessA.port == g_sessB.port) {
        KLOG_ERROR("TTY_MULTI", "fixture: sessions are not distinct.\n");
        failed++;
    }

    /* each port back-references its own session after init */
    if (g_portA.session != &g_sessA || g_portB.session != &g_sessB) {
        KLOG_ERROR("TTY_MULTI", "fixture: port<->session backref wrong.\n");
        failed++;
    }

    if (!test_input_isolation()) {
        failed++;
    }

    if (!test_output_isolation()) {
        failed++;
    }

    if (!test_signal_isolation()) {
        failed++;
    }

    return failed;
}
