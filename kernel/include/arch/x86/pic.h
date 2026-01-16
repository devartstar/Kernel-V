#pragma once
#include "core/io.h"

#define PIC1_COMMAND 0x20 /* Master PIC command port */
#define PIC1_DATA 0x21    /* Master PIC data port */
#define PIC2_COMMAND 0xA0 /* Slave PIC command port */
#define PIC2_DATA 0xA1    /* Slave PIC data port */

#define PIC_EOI 0x20

#define ICW1_ICW4 0x01      /* ICW4 (not) needed */
#define ICW1_SINGLE 0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4 0x04 /* Call address interval 4 (8) */
#define ICW1_LEVEL 0x08     /* Level triggered (edge) mode */
#define ICW1_INIT 0x10      /* Initialization - required! */

#define ICW4_8086 0x01       /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO 0x02       /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE 0x08  /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C /* Buffered mode/master */
#define ICW4_SFNM 0x10       /* Special fully nested (not) */

/**
 * PIC - Programmable Interrupt Controller
 * Hardware that decide which hardware interrupt to send to the CPU
 * IRQs 0-7 are MASTER PIC
 * IRQs 8-15 are SLAVE PIC connected to IRQ2 of MASTER PIC
 */
void pic_init(void);

/**
 * Remap PIC interrupts to new offsets
 * @param offset1 New offset for Master PIC (IRQs 0-7) remapped to INT 32 - 39
 * @param offset2 New offset for Slave PIC (IRQs 8-15) remapped to INT 40 - 47
 */
void pic_remap(int offset1, int offset2);

static inline uint8_t pic_read_mask(uint8_t pic) {
    return inb(pic == 1 ? PIC1_DATA : PIC2_DATA);
}

static inline void pic_write_mask(uint8_t pic, uint8_t mask) {
    outb(pic == 1 ? PIC1_DATA : PIC2_DATA, mask);
}
