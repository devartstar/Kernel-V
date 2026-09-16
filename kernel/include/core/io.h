#ifndef IO_H
#define IO_H

#include <stdint.h>

/*
 * 8-bit version of in out instruction
 */

/**
 * outb - Writes a single byte value to an I/O port
 * stores value -> al & port -> dx register.
 *
 * @port - hardware port to send the value to.
 * @value - value send to the port.
 *
 * @return - void
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

/**
 * inb - Reads a single byte from I/O port and returns it.
 *
 * @port - hardware port to read from.
 *
 * @return - byte value read from I/o port.
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

/*
 * 16-bit version of in out instruction
 */
static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

/*
 * 32-bit version of in out instruction
 */
static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

#endif //  IO_H
