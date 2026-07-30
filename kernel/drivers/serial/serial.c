#include "drivers/serial.h"
#include "arch/x86/interrupt.h"
#include "core/io.h"
#include "fs/vfs.h"

/*
 * The kernel drives two 16550 UARTs so that interactive console traffic and
 * kernel log output never interleave on the same wire:
 *
 *   COM1 (0x3F8) - interactive user console. Backs the /dev/stdout, /dev/stderr
 *                  and /dev/stdin device writes/reads.
 *   COM2 (0x2F8) - kernel log output (KLOG serial backend) only.
 *
 * Register offsets from the port base:
 *   +0 data register (DLAB=0) / divisor low (DLAB=1)
 *   +1 interrupt enable       / divisor high (DLAB=1)
 *   +2 FIFO control
 *   +3 line control
 *   +4 modem control
 *   +5 line status register (bit0 = data ready, bit5 = transmit holding empty)
 */
#define SERIAL_COM1 0x3F8
#define SERIAL_COM2 0x2F8

static void serial_port_init(uint16_t base) {
    /* Disable all interrupts */
    outb(base + 1, 0x00);

    /* Enable DLAB - set baud rate divisor */
    outb(base + 3, 0x80);

    /* Set the divisor low bits - 3 (38400 baud) */
    outb(base + 0, 0x03);

    /* Set the divisor high bits - 0 */
    outb(base + 1, 0x00);

    /* 8 bits, no parity, one stop bit */
    outb(base + 3, 0x03);

    /* Enable FIFO, clear them, 14-byte threshold */
    outb(base + 2, 0xC7);

    /* IRQs enabled, RTS/DSR set */
    outb(base + 4, 0x08);
}

void serial_init(void) {
    serial_port_init(SERIAL_COM1);
    serial_port_init(SERIAL_COM2);
}

/* Transmit FIFO ready to accept a byte */
static int serial_is_transmit_ready(uint16_t base) {
    return inb(base + 5) & 0x20;
}

/* Received data available to read */
static int serial_is_data_ready(uint16_t base) { return inb(base + 5) & 0x01; }

static void serial_port_putc(uint16_t base, char c) {
    while (!serial_is_transmit_ready(base)) {
    };
    outb(base, c);
}

static void serial_port_write(uint16_t base, const char *data, size_t len) {
    irq_flags_t flags = irq_save();
    for (size_t i = 0; i < len; i++) {
        serial_port_putc(base, data[i]);
    }
    irq_restore(flags);
}

/* --- COM1: interactive user console --- */

void serial_putc(char c) { serial_port_putc(SERIAL_COM1, c); }

void serial_write(const char *data, size_t len) {
    serial_port_write(SERIAL_COM1, data, len);
}

/* --- COM2: kernel log output --- */

void serial_log_write(const char *data, size_t len) {
    serial_port_write(SERIAL_COM2, data, len);
}

int serial_getc_nonblocking(char *out_c) {
    /* check for valid arguments */
    if (!out_c) {
        return VFS_ERR_INVALID;
    }

    /* check if data is ready to be read on the console UART */
    if (!serial_is_data_ready(SERIAL_COM1)) {
        return VFS_OK;
    }

    /* read the data */
    *out_c = (char)inb(SERIAL_COM1);
    return 0;
}

uint32_t serial_dump_input_to_console(void) {
    uint32_t dump_count = 0;
    char out_c;

    while (serial_is_data_ready(SERIAL_COM1)) {
        /* Note: Only fails in case of some issue, is serial hardware is not
         * ready it still returns success */
        if (serial_getc_nonblocking(&out_c) == VFS_OK) {
            if (console_input_push(out_c) < 0) {
                dump_count++;
                // KLOG_ERROR("SERIAL", "serial_dump_input_to_console failed.
                // unable to push data to console buffer.\n");
                return dump_count;
            }
        } else {
            // KLOG_ERROR("SERIAL", "serial_dump_input_to_console failed. unable
            // to read data from serial port.\n");
            return dump_count;
        }
    }
    return dump_count;
}
