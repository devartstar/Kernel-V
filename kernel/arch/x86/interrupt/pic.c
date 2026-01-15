#include "arch/x86/pic.h"
#include "lib/printk.h"

void pic_remap(int offset1, int offset2) {
    unsigned char a1, a2;

    /* Save the Interrupt Masks in the PIC data ports
       Each PIC has a data port that stores the interrupt mask
       interrupt mask - which IRQs are enabled/disabled */
    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);

    /* Tell PIC to reset and expect for initialization commands
       Initialization Command Word 1 (ICW1) tells the PICs to start the
       initialization sequence. */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    /* ICW2: Map Master PIC to vector offset1 (32-39) */
    outb(PIC1_DATA, offset1);
    /* ICW2: Map Slave PIC to vector offset2 (40-47) */
    outb(PIC2_DATA, offset2);

    /* ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
     * bitmap */
    outb(PIC1_DATA, 4);
    /* ICW3: tell Slave PIC its cascade identity (0000 0010) connected to PIC 2
     */
    outb(PIC2_DATA, 2);

    /* ICW4: have the PICs use 8086 mode (and not 8080 mode) */
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    /* Restore interrupts enabled/disabled masks back */
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);

    pr_info("[PIC] Remapped to interrupts %d-%d and %d-%d\n", offset1,
            offset1 + 7, offset2, offset2 + 7);
}

void pic_init() {
    /* Remap PIC to use interrupts 32-47 instead of 0-15 */
    pic_remap(32, 40);

    /* Enable timer interrupt (IRQ 0)
       Clear bit 0 to enable IRQ 0 (timer) and set to Master PIC */
    unsigned char mask = inb(PIC1_DATA);
    mask &= ~(1 << 0);
    outb(PIC1_DATA, mask);

    pr_info("[PIC] Initialized successfully!\n");
}

