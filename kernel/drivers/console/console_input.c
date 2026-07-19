#include "drivers/console_input.h"
#include "sync/spinlock.h"

static char g_console_input_buf[CONSOLE_INPUT_BUF_SIZE];
static uint32_t g_console_input_head;
static uint32_t g_console_input_tail;
static uint32_t g_console_input_count;
static spinlock_t g_console_input_lock = SPINLOCK_INIT;

void console_input_init() {
    irq_flags_t flags;
    uint32_t i;

    flags = spin_lock_irqsave(&g_console_input_lock);

    for (i = 0; i < CONSOLE_INPUT_BUF_SIZE; i++) {
        g_console_input_buf[i] = 0;
    }

    g_console_input_head = 0;
    g_console_input_tail = 0;
    g_console_input_count = 0;

    spin_unlock_irqrestore(&g_console_input_lock, flags);
}
