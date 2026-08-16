#include "drivers/serial.h"
#include "arch/x86/interrupt.h"
#include "core/io.h"
#include "drivers/console_input.h"
#include "drivers/tty_console.h"
#include "fs/vfs.h"

/* serial counters */
static volatile uint32_t g_serial_irq_count;
static volatile uint32_t g_serial_rx_byte_count;
static volatile uint32_t g_serial_rx_drop_count;

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

/* serial counters */
uint32_t serial_irq_count(void) { return g_serial_irq_count; }

uint32_t serial_rx_byte_count(void) { return g_serial_rx_byte_count; }

uint32_t serial_rx_drop_count(void) { return g_serial_rx_drop_count; }

/* Transmit FIFO ready to accept a byte */
static int serial_is_transmit_ready(uint16_t base) {
    return inb(base + SERIAL_REG_LSR) & 0x20;
}

/* Received data available to read */
static int serial_is_data_ready(uint16_t base) {
    return inb(base + SERIAL_REG_LSR) & 0x01;
}

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
    return VFS_OK;
}

uint32_t serial_dump_input_to_console(void) {
    uint32_t dump_count = 0;
    char out_c;

    while (serial_is_data_ready(SERIAL_COM1)) {
        /* Note: Only fails in case of some issue, is serial hardware is not
         * ready it still returns success */
        if (serial_getc_nonblocking(&out_c) == VFS_OK) {
            /* Echo the received byte back to COM1 so the user can see what
             * they typed. Translate CR (Enter) into CRLF for a clean line
             * break on the terminal. */
            if (out_c == '\r') {
                serial_putc('\r');
                serial_putc('\n');
            } else {
                serial_putc(out_c);
            }

            if (console_input_push(out_c) != VFS_OK) {
                /* unable to write to console buffer */
                g_serial_rx_drop_count++;

                // KLOG_ERROR("SERIAL", "serial_dump_input_to_console failed.
                // unable to push data to console buffer.\n");
                return dump_count;
            }

            /* successfully wrote to console */
            g_serial_rx_byte_count++;
            dump_count++;
        } else {
            // KLOG_ERROR("SERIAL", "serial_dump_input_to_console failed. unable
            // to read data from serial port.\n");
            return dump_count;
        }
    }
    return dump_count;
}

uint32_t serial_dump_input_to_tty_console(void) {
    char ch;
    uint32_t dump_count = 0;

    while (serial_is_data_ready(SERIAL_COM1)) {
        if (serial_getc_nonblocking(&ch) != VFS_OK) {
            break;
        }
        g_serial_rx_byte_count++;
        dump_count++;
        tty_console_rx((uint8_t)ch);
    }

    return dump_count;
}

/**
 * Flow: Interrupt based Notification
 * Driver enable UART to raise INT. when it gets data.
 * byte arrives at COM1 UART -> UART plases byte in its RX FIFO -> #
 * # -> UART sets LSR.DR = 1 -> UART marks "recieved data available" in IIR -> #
 * # -> UART asserts IRQ4 -> PIC delivers IRQ4 vector to CPU -> #
 * # -> serial interrupt handler runs and reads RBS/RX FIFO
 */

/* --- enable UART to notify CPU with interrupt when data available --- */
void serial_enable_rx_interrupt(uint16_t base) {
    /* enable UART recieved-data-available interrupt */
    outb(base + SERIAL_REG_IER, SERIAL_IER_RX_AVAILABLE);

    /* OUT2 mult be set for UART interrupt signal to reach the PIC */
    outb(base + SERIAL_REG_MCR,
         SERIAL_MCR_DTR | SERIAL_MCR_OUT2 | SERIAL_MCR_RTS);
}

/* --- Serial Interrupt Handler --- */

/**
 * serial_irq_handler - is routine when COM Port signals that data is avaialeble
 * to be read.
 *
 * @idt_idx - interrupt descriptor index for com port interrupt.
 * @reg - saved register context during interrupt
 *
 * @return void
 */
void serial_irq_handler(uint32_t idt_idx, regs_t *reg) {
    (void)idt_idx;
    (void)reg;

    g_serial_irq_count++;

    /* dump the buffer from serial driver to console */
    serial_dump_input_to_console();

    serial_dump_input_to_tty_console();
}
