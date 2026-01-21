#include "arch/x86/interrupt.h"
#include "arch/x86/pic.h"
#include "lib/printk.h"

volatile uint8_t nested_interrupt_count = 0;

static const char *irq_debug_cfg =
#ifdef CONFIG_DEBUG_IRQ_LIST
    CONFIG_DEBUG_IRQ_LIST;
#else
    "";
#endif

#define IRQ_BITMAP_SIZE (IDT_VECTOR_COUNT / 8)
static uint8_t irq_debug_bitmap[IRQ_BITMAP_SIZE] = {0};

#define BIT_SET(n) (irq_debug_bitmap[n >> 3] |= (1 << (n & 7)))
#define BIT_TEST(n) (irq_debug_bitmap[n >> 3] & (1 << (n & 7)))

static int32_t parse_int(const char **p) {
    int32_t val = 0;

    if (**p < '0' || **p > '9') {
        val = -1;
    }

    /* Parse the number */
    while (**p >= '0' && **p <= '9') {
        val = val * 10 + (**p - '0');
        (*p)++;
    }

    return val;
}

void debug_irq_init(void) {
    const char *p = irq_debug_cfg;

    while (*p) {
        int32_t start, end;

        /* Get the starting value */
        start = parse_int(&p);
        if (start < 0 || start >= IDT_VECTOR_COUNT) {
            goto ERROR;
        }

        /* Check if the entry is a range or single entry */
        if (*p == '-') {
            ++p;
            end = parse_int(&p);
            if (end < start || end >= IDT_VECTOR_COUNT) {
                goto ERROR;
            }
        } else {
            end = start;
        }

        /* Set the bits in the bitmap for all the values */
        for (int32_t i = start; i <= end; i++) {
            BIT_SET(i);
        }

        /* Check if more entries are present */
        if (*p == ',') {
            p++;
        } else if (*p == '\0') {
            break;
        } else {
            goto ERROR;
        }
    }

    return;

ERROR:
    /* Fail closed: disable IRQ debugging */
    for (int i = 0; i < IRQ_BITMAP_SIZE; i++)
        irq_debug_bitmap[i] = 0;

    pr_info("IRQ debug: invalid config string '%s'\n", irq_debug_cfg);
}

bool is_irq_debug_enabled(uint32_t idt_index) {
    if (idt_index >= IDT_VECTOR_COUNT) {
        return false;
    }

    return BIT_TEST(idt_index);
}

void irq_mask(uint8_t irq) {
    uint8_t value;

    if (irq < 8) {
        value = current_pic1_mask | (1 << irq);
        if (value != current_pic1_mask) {
            current_pic1_mask = value;
            pic_write_mask(1, value);
        }
    } else {
        irq -= 8;
        value = current_pic2_mask | (1 << irq);
        if (value != current_pic2_mask) {
            current_pic2_mask = value;
            pic_write_mask(2, value);
        }
    }
}

void irq_unmask(uint8_t irq) {
    uint8_t value;

    if (irq < 8) {
        value = current_pic1_mask & ~(1 << irq);
        if (value != current_pic1_mask) {
            current_pic1_mask = value;
            pic_write_mask(1, current_pic1_mask);
        }
    } else {
        irq -= 8;
        value = current_pic2_mask & ~(1 << irq);
        if (value != current_pic2_mask) {
            current_pic2_mask = value;
            pic_write_mask(2, current_pic2_mask);
        }
    }
}

void irq_mask_all(void) {
    current_pic1_mask = 0xFF;
    current_pic2_mask = 0xFF;

    outb(PIC1_DATA, current_pic1_mask);
    outb(PIC2_DATA, current_pic2_mask);
}

void irq_unmask_all(void) {
    current_pic1_mask = 0x00;
    current_pic2_mask = 0x00;

    outb(PIC1_DATA, current_pic1_mask);
    outb(PIC2_DATA, current_pic2_mask);
}

void irq_mask_all_but(const uint8_t *whitelist, uint8_t n) {
    uint8_t mask1 = 0xFF, mask2 = 0xFF;
    for (size_t i = 0; i < n; ++i) {
        uint8_t irq = whitelist[i];
        if (irq < 8)
            mask1 &= ~(1 << irq);
        else
            mask2 &= ~(1 << (irq - 8));
    }
    if (mask1 != current_pic1_mask) {
        current_pic1_mask = mask1;
        outb(PIC1_DATA, mask1);
    }
    if (mask2 != current_pic2_mask) {
        current_pic2_mask = mask2;
        outb(PIC2_DATA, mask2);
    }
}

int irq_is_masked(uint8_t irq) {
    if (irq < 8)
        return (current_pic1_mask & (1 << irq)) != 0;
    else
        return (current_pic2_mask & (1 << (irq - 8))) != 0;
}

void print_irq_masks(void) {
    KLOG_INFO("PIC", "PIC1 IMR=0x%02x, PIC2 IMR=0x%02x (sw tracked)\n",
              current_pic1_mask, current_pic2_mask);
    KLOG_INFO("PIC", "HW  PIC1 IMR=0x%02x, PIC2 IMR=0x%02x (actual)\n",
              inb(PIC1_DATA), inb(PIC2_DATA));
}
