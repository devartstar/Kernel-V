#include "drivers/tty_console.h"
#include "drivers/serial.h"
#include "drivers/tty_pipeline.h"
#include "drivers/tty_port.h"
#include "drivers/tty_session.h"
#include "drivers/tty_stage.h"
#include "drivers/tty_stage_echo.h"
#include "drivers/tty_stage_icrnl.h"
#include "drivers/tty_stage_onlcr.h"
#include "drivers/vga.h"
#include "fs/devfs.h"
#include "lib/printk.h"

static void console_port_putc(tty_port_t *port, uint8_t byte);

static tty_session_t g_console_session;
static tty_port_t g_console_port;
static const tty_port_ops_t g_console_port_ops = {.putc = console_port_putc};
static echo_state_t g_console_echo_state;

/* Line Discipline */
static tty_stage_t g_console_out_stages[1];
static tty_pipeline_t g_console_out_pipeline;

static tty_stage_t g_console_in_stages[2];
static tty_pipeline_t g_console_in_pipeline;

static void console_port_putc(tty_port_t *port, uint8_t byte) {
    (void)port;
    vga_put_char((char)byte, DEVFS_CONSOLE_COLOR);
    serial_putc((char)byte);
}

int tty_console_init() {
    int ret;
    g_console_port.ops = &g_console_port_ops;
    g_console_port.session = NULL;
    g_console_port.private_data = NULL;
    /* Initialize the line discipline pipeline */
    /* [1] Initialize the onlcr and icrln stages */
    g_console_out_stages[0] = tty_stage_onlcr_make();

    /**
     * Why `[icrnl] → [echo]` and not `[echo] → [icrnl]`? What happens when user
     * press Enter ?
     * Case 1: [icrnl, echo]: Enter's `\r` becomes `\n` *first*, so echo sees
     * `\n` and displays a proper newline (via onlcr → `\r\n`). Cursor moves to
     * the next line.
     *
     * Case 2: [echo, icrnl]: echo sees the raw `\r`, displays
     * just a carriage return (cursor to column 0, no line down), then icrnl
     * converts it. In Case 2 - The visible result is wrong — the cursor jumps
     * to column 0 but doesn't advance a line.
     */
    g_console_in_stages[0] = tty_stage_icrnl_make();
    g_console_in_stages[1] =
        tty_stage_echo_make(&g_console_echo_state, &g_console_out_pipeline,
                            tty_port_sink, &g_console_port);

    /* [2] Initialize the output pipeline obj */
    g_console_out_pipeline.stages = g_console_out_stages;
    g_console_out_pipeline.count = 1;

    /* [3] Initialize the input pipeline obj */
    g_console_in_pipeline.stages = g_console_in_stages;
    g_console_in_pipeline.count = 2;

    ret = tty_session_init(&g_console_session, &g_console_port);
    if (ret != TTY_CHAN_OK) {
        KLOG_ERROR("TTY", " tty console initialization failed. err=%d\n", ret);
        return ret;
    }

    /* [4] Attach the input & output pipeline to the session */
    g_console_session.in_pipeline = &g_console_in_pipeline;
    g_console_session.out_pipeline = &g_console_out_pipeline;

    KLOG_INFO("TTY",
              "tty console initialization completed. session=%p, port=%p.\n",
              (void *)&g_console_session, (void *)&g_console_port);
    return TTY_CHAN_OK;
}

void tty_console_rx(uint8_t byte) { tty_port_rx(&g_console_port, byte); }

tty_session_t *tty_console_session(void) { return &g_console_session; }
