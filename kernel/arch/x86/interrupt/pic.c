#include "core/io.h"
#include "lib/printk.h"

#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

#define PIC_EOI         0x20

#define ICW1_ICW4       0x01    /* ICW4 (not) needed */
#define ICW1_SINGLE     0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4  0x04    /* Call address interval 4 (8) */
#define ICW1_LEVEL      0x08    /* Level triggered (edge) mode */
#define ICW1_INIT       0x10    /* Initialization - required! */

#define ICW4_8086       0x01    /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO       0x02    /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE  0x08    /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C    /* Buffered mode/master */
#define ICW4_SFNM       0x10    /* Special fully nested (not) */

void pic_remap(int offset1, int offset2)
{
    unsigned char a1, a2;
    
    // Save masks
    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);
    
    // Start initialization sequence (in cascade mode)
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    
    // ICW2: Master PIC vector offset
    outb(PIC1_DATA, offset1);
    // ICW2: Slave PIC vector offset
    outb(PIC2_DATA, offset2);
    
    // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    outb(PIC1_DATA, 4);
    // ICW3: tell Slave PIC its cascade identity (0000 0010)
    outb(PIC2_DATA, 2);
    
    // ICW4: have the PICs use 8086 mode (and not 8080 mode)
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);
    
    // Restore saved masks
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
    
    pr_info("[PIC] Remapped to interrupts %d-%d and %d-%d\n", 
            offset1, offset1 + 7, offset2, offset2 + 7);
}

void pic_init()
{
    // Remap PIC to use interrupts 32-47 instead of 0-15
    pic_remap(32, 40);
    
    // Enable timer interrupt (IRQ 0)
    unsigned char mask = inb(PIC1_DATA);
    mask &= ~(1 << 0);  // Clear bit 0 to enable IRQ 0 (timer)
    outb(PIC1_DATA, mask);
    
    pr_info("[PIC] Initialized successfully!\n");
}