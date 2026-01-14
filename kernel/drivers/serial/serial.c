#include "drivers/serial.h"
#include "core/io.h"

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

void serial_putc(char c) {
    while (!serial_is_transmit_ready) {
    };
    outb(SERIAL_PORT, c);
}

void serial_write(const char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        serial_putc(data[i]);
    }
}
