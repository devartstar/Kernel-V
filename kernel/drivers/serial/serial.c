#include "drivers/serial.h"
#include "arch/x86/interrupt.h"
#include "core/io.h"
#include "fs/vfs.h"

/**
 * SERIAL_PORT + 0 -> data register
 * SERIAL_PORT + 5 -> line status register
 */
#define SERIAL_PORT 0x3F8

void serial_init(void) {
    /* Disable all interrupts */
    outb(SERIAL_PORT + 1, 0x00);

    /* Enable DLAB - set baud rate divisor */
    outb(SERIAL_PORT + 3, 0x80);

    /* Set the divisor low bits - 3 */
    outb(SERIAL_PORT + 0, 0x03);

    /* Set the divisor high bits - 0 */
    outb(SERIAL_PORT + 1, 0x00);

    /* 8 bits, no parity, one stop bit */
    outb(SERIAL_PORT + 3, 0x03);

    /* Enable FIFO, clear them, 14-byte threshold */
    outb(SERIAL_PORT + 2, 0xC7);

    /* IRQs enabled, RTS/DSR set */
    outb(SERIAL_PORT + 4, 0x08);
}

/* Wait for transmit FIFO to be ready */
static int serial_is_transmit_ready() { return inb(SERIAL_PORT + 5) & 0x20; }

/* Wait for data to be available to read */
static int serial_is_data_ready() { return inb(SERIAL_PORT + 5) & 0x01; }

void serial_putc(char c) {
    while (!serial_is_transmit_ready()) {
    };
    outb(SERIAL_PORT, c);
}

void serial_write(const char *data, size_t len) {
    irq_flags_t flags = irq_save();
    for (size_t i = 0; i < len; i++) {
        serial_putc(data[i]);
    }
    irq_restore(flags);
}

int serial_getc_nonblocking(char *out_c) {
    /* check for valid arguments */
    if (!out_c) {
        KLOG_ERROR("SERIAL", "serial_getc_nonblocking failed. out_c is NULL.\n");
        return VFS_ERR_INVALID;
   }

   /* check if data is ready to be read */
   if(!serial_is_data_ready()) {
        KLOG_ERROR("SERIAL", "serial_getc_nonblocking failed. no data available to read.\n");
        return VFS_OK;
   }

    /* read the data */
    *out_c = (char)inb(SERIAL_PORT);
    return 0;
}

uint32_t serial_dump_input_to_console(void) {
    uint32_t dump_count = 0;
    char out_c;

    while (serial_is_data_ready()) {
        /* Note: Only fails in case of some issue, is serial hardware is not ready it still returns success */
        if (serial_getc_nonblocking(&out_c) == VFS_OK) {
            if(console_input_push(out_c) < 0) {
                dump_count++;
                KLOG_ERROR("SERIAL", "serial_dump_input_to_console failed. unable to push data to console buffer.\n");
                return dump_count;
            }
        }
        else {
            KLOG_ERROR("SERIAL", "serial_dump_input_to_console failed. unable to read data from serial port.\n");
            return dump_count;
        }
    }
    return dump_count;
}