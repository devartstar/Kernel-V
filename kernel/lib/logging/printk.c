#include "lib/printk.h"
#include "drivers/serial.h"
#include "drivers/vga.h"
#include "lib/logbackend.h"
#include "lib/string.h"
#include "proc/proc.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

extern volatile uint32_t tick_count;

/* Circular log buffer for storing kernel messages */
static char log_buffer[LOG_BUF_SIZE];

/* Ring buffer pointers */
static size_t rb_head = 0; //  Position of next byte to be written
static size_t rb_tail = 0; //  Position of oldest byte in buffer

/* Log levels for kernel messages */
const struct loglevel loglevels[] = {
    {'0', "EMERG", VGA_COLOR(VGA_BLACK, VGA_RED)},
    {'1', "ERROR", VGA_COLOR(VGA_BLACK, VGA_YELLOW)},
    {'2', "WARN", VGA_COLOR(VGA_BLACK, VGA_WHITE)},
    {'3', "INFO", VGA_COLOR(VGA_BLACK, VGA_DARK_GREY)},
    {'4', "VERBOSE", VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN)}};

const int num_loglevels = sizeof(loglevels) / sizeof(loglevels[0]);

/**
 * ringbuf_putc - Append one character to the circular ring buffer
 * @ch - character to append
 * @returns void
 */
static void ringbuf_putc(char ch) {
    log_buffer[rb_head] = ch;
    rb_head = (rb_head + 1) % LOG_BUF_SIZE;

    /* If buffer is full, advance tail to drop oldest byte */
    if (rb_head == rb_tail) {
        rb_tail = (rb_tail + 1) % LOG_BUF_SIZE;
    }
}

void ringbuf_write(const char *str, size_t str_len) {
    for (size_t i = 0; i < str_len; i++) {
        ringbuf_putc(str[i]);
    }
}

/**
 * find_loglevel - Find log level by character.
 * @level_char - character representing log level.
 * @returns index of log level in loglevels array or -1 if not found.
 */
static int find_loglevel(char level_char) {
    for (int i = 0; i < num_loglevels; i++) {
        if (loglevels[i].level_char == level_char) {
            return i;
        }
    }
    return -1;
}

void printk_init(void) {
    rb_head = 0;
    rb_tail = 0;
    vga_init();
    serial_init();

    num_logs_backend = 0;
    register_log_backend(vga_backend);
    register_log_backend(ringbuf_backend);
    register_log_backend(serial_backend);
}

/**
 * vprintk - Internal printk function handling arguments va_list.
 * @fmt - format string to be printed.
 * @args - variable argument list.
 * @returns number of characters printed.
 */
int vprintk(const char *fmt, va_list args) {
    char tmp[LOG_BUF_SIZE];

    int len = my_vsnprintf(tmp, sizeof(tmp), fmt, args);

    log_dispatch_to_backends(tmp, len, WHITE_ON_BLACK);

    return len;
}

int printk(const char *fmt, ...) {
    char level_prefix[32];
    const char *actual_fmt = fmt;
    int log_level_idx = -1;
    int total_len = 0;

    if (fmt[0] == '\001' && fmt[1] >= '0' && fmt[1] <= '7') {
        int idx = find_loglevel(fmt[1]);
        if (idx >= 0) {
            log_level_idx = idx;
        }
        actual_fmt = fmt + 2;
    }

    if (log_level_idx != -1 || log_level_idx > CONFIG_TRACE_LEVEL) {
        return 0;
    }

    if (log_level_idx != -1) {
        //  Build the level prefix: [LEVEL]
        char *p = level_prefix;
        *p++ = '[';

        //  Copy the correct log level name
        const char *name = loglevels[log_level_idx].name;
        while (*name) {
            *p++ = *name++;
        }

        *p++ = ']';
        *p++ = ' ';
        *p = '\0';

        int prefix_len = p - level_prefix;

        //  Write to ring buffer and console with colored prefix
        log_dispatch_to_backends(level_prefix, prefix_len,
                                 loglevels[log_level_idx].color);
        total_len += prefix_len;
    }

    va_list args;
    va_start(args, fmt);
    int msg_len = vprintk(actual_fmt, args);
    va_end(args);

    total_len += msg_len;
    return total_len;
}

int printk_structured(const char *level, const char *tag, const char *file,
                      const char *func, int line, const char *fmt, ...) {
    char logbuf[512];
    int log_level_idx = -1;

    const char *level_str = level;
    if (level_str[0] == '\001' && level_str[1] >= '0' && level_str[1] <= '7') {
        int idx = find_loglevel(level_str[1]);
        if (idx >= 0)
            log_level_idx = idx;
        level_str += 2; // skip for user output, not needed in prefix
    }

    /* Invalid log level or log level more than config threshold set to print*/
    if (log_level_idx < 0 || log_level_idx > CONFIG_TRACE_LEVEL) {
        return 0;
    }

    const char *level_name = loglevels[log_level_idx].name;
    uint32_t tick = tick_count;
    int pid = current_proc ? (int)current_proc->pid : -1;
    const char *pname = current_proc ? current_proc->name : "?";
    int cpu = 0;

    /* Structuring the log prefix */
    int prefix_len = my_snprintf(
        logbuf, sizeof(logbuf), "[%s][%lu][pid=%d:%s][cpu=%d][%s:%s:%d][%s] ",
        level_name, tick, pid, pname, cpu, file, func, line, tag);

    /* Structuring the user log message */
    va_list ap;
    va_start(ap, fmt);
    int message_len =
        my_vsnprintf(logbuf + prefix_len, sizeof(logbuf) - prefix_len, fmt, ap);
    va_end(ap);

    log_dispatch_to_backends(logbuf, prefix_len + message_len,
                             loglevels[log_level_idx].color);

    return prefix_len + message_len;
}
