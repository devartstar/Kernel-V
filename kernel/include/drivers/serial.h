#pragma once

/**
 * 16550 - compatible UART register map
 * ===========================================================================
 * COM Port is a UART device exposed to the CPU thru 8 consecutive IO address
 * COM1 Base address = 0x3F8 -> 0x3FF - each IO address represents a hardware
 *state location. [Q]
 *
 * Some IO Port address select different UART registers based on:
 *	1. CPU IO instruction inb() / outb()
 *	2. DLAB (LSR - bit 7) value. DLAB = Divisor Latch access bit.
 *
 *	Base Address + Offset = Register
 * +--------+------+----------------------------+---------------------------+
 * | Offset | DLAB | Read using inb()			| Write using outb()		|
 * +--------+------+----------------------------+---------------------------+
 * | base+0 |  0   | RBR - Receiver Buffer		| THR - Transmit Holding	|
 * | base+1 |  0   | IER - Interrupt Enable		| IER - Interrupt Enable	|
 * | base+0 |  1   | DLL - Divisor Latch Low	| DLL - Divisor Latch Low	|
 * | base+1 |  1   | DLM - Divisor Latch High	| DLM - Divisor Latch High	|
 * | base+2 | any  | IIR - Interrupt ID         | FCR - FIFO Control        |
 * | base+3 | any  | LCR - Line Control         | LCR - Line Control        |
 * | base+4 | any  | MCR - Modem Control        | MCR - Modem Control       |
 * | base+5 | any  | LSR - Line Status          | Normally read-only        |
 * | base+6 | any  | MSR - Modem Status         | Normally read-only        |
 * | base+7 | any  | SCR - Scratch              | SCR - Scratch				|
 * +--------+------+----------------------------+---------------------------+
 *
 * NOTE:: DLAB must be cleared before normal transmission and reception. DLAB=0
 * inb(base + 0) -> read recieved byte from RBR/RX FIFO
 * outb(base + 0) -> write transmit bte to THR/TX FIFO
 *
 * inb(base + 2) -> read interrupt reason from IIR
 *
 * Baud Rate: number of serial synmobols transmitted per second.
 * Both Transmitter and Reciever Baud Rate must be same.
 *		divisor =  (DLM << 8) | DLL
 *		baud = 115200 / divisor
 * Sending 1 byte of data takes 10 bits (Start + 8 data + End) bits
 * Baud rate = 115200 => 115200 / 10 -> 11520 bytes/s
 */

/**
 * RBR - Bits 7:0 are recieved bytes
 * THR - Bits 7:0 are transmitted bits
 * DLL / DML are usually clubbed together 15:0 for bud rate divisor
 * IER - Selects UART events which can generate IRQ
 * IIR - If interrupt is pending and its cause
 * FCR - Enable /  Disbales and configures FIFO.
 * LCR - configure frame format and select divisor register
 * LSR - reports reciever, transmitter, and error status.
 * SCR - general purpose 8 bit register
 */

/**
 * IER — Interrupt Enable Register (base + 1, DLAB=0)
 *
 * bit 0  ERBFI  Enable received-data interrupt
 * bit 1  ETBEI  Enable transmitter-empty interrupt
 * bit 2  ELSI   Enable receiver-line-status interrupt
 * bit 3  EDSSI  Enable modem-status interrupt
 *
 * Kernel-V commonly needs:
 *     bit 0 = 1    receive bytes using interrupts
 */

/**
 * IIR — Interrupt Identification Register (read base + 2)
 *
 * bit 0      Interrupt status:
 *              0 = interrupt pending
 *              1 = no interrupt pending
 *
 * bits 3:1  Interrupt reason:
 *              011 = receiver line status       (IIR & 0x0F = 0x06)
 *              010 = received data available    (IIR & 0x0F = 0x04)
 *              110 = receive timeout            (IIR & 0x0F = 0x0C)
 *              001 = transmitter empty          (IIR & 0x0F = 0x02)
 *              000 = modem status               (IIR & 0x0F = 0x00)
 *
 * bits 7:6  FIFO status
 */

/**
 * FCR — FIFO Control Register (write base + 2)
 *
 * bit 0      Enable RX and TX FIFOs
 * bit 1      Clear RX FIFO
 * bit 2      Clear TX FIFO
 * bit 3      DMA mode
 * bits 7:6   RX interrupt trigger level:
 *              00 = 1 byte
 *              01 = 4 bytes
 *              10 = 8 bytes
 *              11 = 14 bytes
 *
 * Common value:
 *     0xC7 = enable FIFOs, clear both, trigger at 14 bytes
 */

/**
 * LCR — Line Control Register (base + 3)
 *
 * bits 1:0  Character length:
 *              00 = 5 bits
 *              01 = 6 bits
 *              10 = 7 bits
 *              11 = 8 bits
 *
 * bit 2      Stop-bit selection
 * bit 3      Enable parity
 * bit 4      Even/odd parity selection
 * bit 5      Stick parity
 * bit 6      Send break
 * bit 7      DLAB:
 *              0 = access RBR/THR and IER
 *              1 = access DLL and DLM
 *
 * Common value:
 *     0x03 = 8 data bits, no parity, 1 stop bit (8N1)
 */

/**
 * MCR — Modem Control Register (base + 4)
 *
 * bit 0  DTR       Data Terminal Ready
 * bit 1  RTS       Request To Send
 * bit 2  OUT1      Auxiliary output
 * bit 3  OUT2      Enables UART IRQ routing on traditional PCs
 * bit 4  LOOP      Enable internal loopback mode
 * bit 5  AFE       Automatic flow control, if supported
 *
 * Common value:
 *     0x0B = DTR | RTS | OUT2
 */

/**
 * LSR - Line Status Register (read base + 5)
 *
 * Bit:   7       6       5       4      3      2      1      0
 *      ┌───────┬──────┬──────┬──────┬──────┬──────┬──────┬────┐
 * LSR: │FIFOERR│ TEMT │ THRE │  BI  │  FE  │  PE  │  OE  │ DR │
 *      └───────┴──────┴──────┴──────┴──────┴──────┴──────┴────┘
 *
 * Bit 0 -> Data Ready (DR) - recieved data is available to read
 * Bit 1 -> Overrun Error (OE) - new data has arrived, but the previous data was
 * not read yet
 * Bit 2 -> Parity Error (PE) - the received byte had parity error
 * Bit 3 -> Framing Error (FE) - the received byte did not have a valid stop bit
 * Bit 4 -> Break Interrupt (BI) - the received byte was a break signal
 * Bit 5 -> Transmitter Holding Register Empty (THRE) - the transmitter is ready
 * to accept a new byte to send
 * Bit 6 -> Transmitter Empty (TEMT) - the transmitter and its shift register
 * are empty
 * Bit 7 -> FIFO Error (FIFOERR) - the FIFO has encountered an error
 *
 * Serial line -> UART RX FIFO (small hardware queue) -> Serial IQR handler
 * -> TTY/console input buffer (larger kernel queue) -> process read
 */

/**
 * MSR — Modem Status Register (base + 6)
 *
 * bits 3:0  Changes in modem-input signals
 * bit 4     CTS — Clear To Send
 * bit 5     DSR — Data Set Ready
 * bit 6     RI  — Ring Indicator
 * bit 7     DCD — Data Carrier Detect
 *
 * Usually unnecessary for a basic QEMU serial console.
 */

#include "arch/x86/interrupt.h"
#include <stddef.h>
#include <stdint.h>

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

/**
 * SERIAL_PORT + 0  = data register
 * SERIAL_PORT + 1  = interrupt enable register, IER
 * SERIAL_PORT + 2  = interrupt identification register, IIR
 * SERIAL_PORT + 4  = modem control register, MCR
 * SERIAL_PORT + 5  = line status register, LSR
 */
#define SERIAL_REG_DATA 0
#define SERIAL_REG_IER 1
#define SERIAL_REG_ITR 2
#define SERIAL_REG_FCR 2
#define SERIAL_REG_MCR 4
#define SERIAL_REG_LSR 5

/* some bits of these registers */
#define SERIAL_IER_RX_AVAILABLE 0x01
#define SERIAL_MCR_DTR 0x01
#define SERIAL_MCR_RTS 0x02
#define SERIAL_MCR_OUT2 0x08

/* IRQ line for COM1 */
#define SERIAL_IRQ_COM1 4

void serial_init(void);
void serial_write(const char *data, size_t len);
void serial_putc(char c);

/**
 * serial_log_write - write kernel log bytes to the dedicated log UART (COM2).
 * Kept separate from serial_write (COM1 console) so KLOG output never
 * interleaves with interactive stdin/stdout traffic.
 *
 * @param data - buffer of bytes to emit on the log UART.
 * @param len  - number of bytes to write.
 */
void serial_log_write(const char *data, size_t len);

/**
 * serial_getc_nonblocking - reads a single byte from the serial port if
 * available. if the data is not available, it returns with an expected error
 * code.
 *
 * @param out_c - pointer to the character variable where the read byte will be
 * stored.
 *
 * @return - 0 on success, or a negative error code on failure.
 */
int serial_getc_nonblocking(char *out_c);

/**
 * serial_dump_input_to_console - reads all available data from the serial port
 * and writes it to the console. UART - hardware which collects the serial bits
 * and transform to CPU understandable bytes. vice-versa. Serial Driver - kernel
 * driver which control the UART hardware using IO ports. IO Port - SERIAL_PORT
 * + 0 -> transfer the bytes to serial driver, SERIAL_PORT + 5 -> check if bytes
 * ready to read. Console Buffer - kernel buffer which stores the bytes read
 * from serial driver and provides to the user space.
 *
 * @return - the number of bytes successfully dumped to the console buffer.
 */
uint32_t serial_dum_input_to_console(void);

/**
 * serial_enable_rx_interrupt - driver enabled the UART to allow assert
 * interrupt when data available.
 *
 * @base - base address of the COM port
 *
 * @return void
 */
void serial_enable_rx_interrupt(uint16_t base);

/**
 * serial_irq_handler - ISR routine invoked when a COM port signals that data is
 * available to be read (IRQ4 for COM1).
 *
 * @idt_idx - interrupt descriptor index for the COM port interrupt.
 * @reg     - saved register context captured during the interrupt.
 *
 * @return void
 */
void serial_irq_handler(uint32_t idt_idx, regs_t *reg);

/* global counter accessors */
uint32_t serial_irq_count(void);
uint32_t serial_rx_byte_count(void);
uint32_t serial_rx_drop_count(void);
