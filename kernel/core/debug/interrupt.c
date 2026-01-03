#include "arch/x86/interrupt.h"
#include "lib/printk.h"

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
