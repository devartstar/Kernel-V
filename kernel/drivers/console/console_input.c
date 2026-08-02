#include "drivers/console_input.h"
#include "fs/vfs.h"
#include "lib/printk.h"
#include "sync/spinlock.h"

/**
 * g_console_input_buf - a managed circular buffer - defined usinf an array of
 * characters with a head(g_console_input_head) and tail(g_console_input_tail).
 * when a byte is pushed into the buffer, head is incremented
 * when a byte is popped out of the buffer, tail is incremented
 *
 * g_console_input_count - number of characters in between head and tail.
 * if g_console_input_count >= CONSOLE_INPUT_BUF_SIZE - means the buffer is full
 * addition of bytes to the buffer will be dropped with NOMEM error.
 */

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

uint32_t console_input_available() {
    irq_flags_t flags;
    uint32_t count;

    flags = spin_lock_irqsave(&g_console_input_lock);
    count = g_console_input_count;
    spin_unlock_irqrestore(&g_console_input_lock, flags);

    return count;
}

int console_input_push(char in_c) {
    irq_flags_t flags;

    /* take a lock on the global structures */
    flags = spin_lock_irqsave(&g_console_input_lock);

    /* check the current console buffer size and handle overflow */
    if (g_console_input_count >= CONSOLE_INPUT_BUF_SIZE) {
        KLOG_ERROR("CONSOLE", "console buffer is full (%u/%u).\n",
                   g_console_input_count, (uint32_t)CONSOLE_INPUT_BUF_SIZE);
        spin_unlock_irqrestore(&g_console_input_lock, flags);
        return VFS_ERR_NOMEM;
    }

    /* add the character to the head of the buffer */
    g_console_input_buf[g_console_input_head++] = in_c;

    /* update the head accordingly, consider overflow case */
    if (g_console_input_head == CONSOLE_INPUT_BUF_SIZE) {
        g_console_input_head %= CONSOLE_INPUT_BUF_SIZE;
    }

    /* update the count of characters in the buffer */
    g_console_input_count++;
    KLOG_VERBOSE("CONSOLE_PUSH", "Added char %c, size = %u.\n", in_c,
                 g_console_input_count);

    /* release the lock */
    spin_unlock_irqrestore(&g_console_input_lock, flags);

    return VFS_OK;
}

int console_input_pop(char *out_c) {
    irq_flags_t flags;

    /* check for valid reference to store the output character */
    if (!out_c) {
        KLOG_ERROR("CONSOLE",
                   "invalid reference to store the output character.\n");
        return VFS_ERR_INVALID;
    }

    /* take a lock before operating on the global structures */
    flags = spin_lock_irqsave(&g_console_input_lock);

    /* check if the console buffer is null or contains no characters */
    if (g_console_input_count == 0) {
        KLOG_ERROR("CONSOLE",
                   "console_input_pop failed. console buffer is empty.\n");
        spin_unlock_irqrestore(&g_console_input_lock, flags);
        return VFS_ERR_NOTFOUND;
    }

    /* pop the character from buffer to the output reference */
    *out_c = g_console_input_buf[g_console_input_tail++];

    /* check for overflow of the tail */
    g_console_input_tail %= CONSOLE_INPUT_BUF_SIZE;

    /* update the count of characters in the buffer */
    g_console_input_count--;

    KLOG_VERBOSE("CONSOLE_POP", "out character = %c, left size = %u.\n", *out_c,
                 g_console_input_count);

    /* release the lock */
    spin_unlock_irqrestore(&g_console_input_lock, flags);

    return VFS_OK;
}

int console_input_read(char *buf, uint32_t len) {
    uint32_t read_len = 0;
    int ret = VFS_OK;

    if (!buf && len > 0) {
        KLOG_ERROR("CONSOLE",
                   "console read failed. invalid buffer reference.\n");
        return VFS_ERR_INVALID;
    }

    /* invoke the console pop characters until read upto len */
    while (read_len < len) {
        ret = console_input_pop(&buf[read_len]);
        if (ret != VFS_OK) {
            break;
        }
        read_len++;
        KLOG_VERBOSE("CONSOLE_READ",
                     "read length = %u, char = %c, read buffer = %.*s.\n",
                     read_len, buf[read_len - 1], (int)read_len, buf);
    }

    return (int)read_len;
}
