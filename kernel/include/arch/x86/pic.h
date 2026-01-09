#pragma once

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