#pragma once

/**
 * SERIAL_PORT + 5 -> line status register
 * 
 * Bit:   7       6       5       4      3      2      1      0
 *      ┌───────┬──────┬──────┬──────┬──────┬──────┬──────┬────┐
 * LSR: │FIFOERR│ TEMT │ THRE │  BI  │  FE  │  PE  │  OE  │ DR │
 *      └───────┴──────┴──────┴──────┴──────┴──────┴──────┴────┘
 * 
 * Bit 0 -> Data Ready (DR) - recieved data is available to read
 * Bit 1 -> Overrun Error (OE) - new data has arrived, but the previous data was not read yet
 * Bit 2 -> Parity Error (PE) - the received byte had parity error
 * Bit 3 -> Framing Error (FE) - the received byte did not have a valid stop bit
 * Bit 4 -> Break Interrupt (BI) - the received byte was a break signal
 * Bit 5 -> Transmitter Holding Register Empty (THRE) - the transmitter is ready to accept a new byte to send
 * Bit 6 -> Transmitter Empty (TEMT) - the transmitter and its shift register are empty
 * Bit 7 -> FIFO Error (FIFOERR) - the FIFO has encountered an error
 */

#include <stddef.h>
#include <stdint.h>

void serial_init(void);
void serial_write(const char *data, size_t len);
void serial_putc(char c);

/**
 * serial_getc_nonblocking - reads a single byte from the serial port if available.
 * if the data is not available, it returns with an expected error code.
 * 
 * @param out_c - pointer to the character variable where the read byte will be stored.
 * 
 * @return - 0 on success, or a negative error code on failure.
 */
int serial_getc_nonblocking(char *out_c);

/**
 * serial_dump_input_to_console - reads all available data from the serial port and writes it to the console.
 * UART - hardware which collects the serial bits and transform to CPU understandable bytes. vice-versa.
 * Serial Driver - kernel driver which control the UART hardware using IO ports.
 *  IO Port - SERIAL_PORT + 0 -> transfer the bytes to serial driver, SERIAL_PORT + 5 -> check if bytes ready to read.
 * Console Buffer - kernel buffer which stores the bytes read from serial driver and provides to the user space.
 * 
 * @return - the number of bytes successfully dumped to the console buffer.
 */
uint32_t serial_dump_input_to_console(void);
