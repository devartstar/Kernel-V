#include "drivers/tty_console.h"
#include "drivers/serial.h"
#include "drivers/tty_pipeline.h"
#include "drivers/tty_session.h"
#include "drivers/tty_stage.h"
#include "drivers/tty_stage_onlcr.h"
#include "drivers/vga.h"
#include "fs/devfs.h"
#include "lib/printk.h"

static void console_port_putc(tty_port_t *port, uint8_t byte);

static tty_session_t g_console_session;
static tty_port_t g_console_port;
static const tty_port_ops_t g_console_port_ops = {.putc = console_port_putc};

/* Line Discipline */
static tty_stage_t g_console_out_stages[1];
static tty_pipeline_t g_console_out_pipeline;

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
    /* [1] Initialize the onlcr stage */
    g_console_out_stages[0] = tty_stage_onlcr_make();

    /* [2] Initialize the pipeline obj */
    g_console_out_pipeline.stages = g_console_out_stages;
    g_console_out_pipeline.count = 1;

    ret = tty_session_init(&g_console_session, &g_console_port);
    if (ret != TTY_CHAN_OK) {
        KLOG_ERROR("TTY", " tty console initialization failed. err=%d\n", ret);
        return ret;
    }

    /* Attach the pipeline to the session */
    g_console_session.out_pipeline = &g_console_out_pipeline;

    KLOG_INFO("TTY",
              "tty console initialization completed. session=%p, port=%p.\n",
              (void *)&g_console_session, (void *)&g_console_port);
    return TTY_CHAN_OK;
}

void tty_console_rx(uint8_t byte) { tty_port_rx(&g_console_port, byte); }

tty_session_t *tty_console_session(void) { return &g_console_session; }
