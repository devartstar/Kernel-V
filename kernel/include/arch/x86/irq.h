#ifndef IRQ_H
#define IRQ_H

#include "arch/x86/interrupt.h"
#include "arch/x86/pic.h"
#include "lib/printk.h"
#include <stdint.h>

#define IRQ_LINE_COUNT 16
#define IRQ_BASE_VECTOR 32

/**
 * irq_result_t
 */
typedef enum irq_result {
    IRQ_NONE = 0,
    IRQ_HANDLED = 1,
} irq_result_t;

/**
 * Pointer to a handler function for the hardware iqr with given context
 */
typedef irq_result_t (*irq_handler_fn_t)(void *ctx);

/**
 * irq_action - structure for each irq with handler and other metadata
 */
typedef struct irq_action {
    irq_handler_fn_t handler;
    void *ctx;
    const char *name;
    uint8_t registered;
} irq_action_t;

/**
 * irq_action - table for irq_action_t for each irq_line
 */
static irq_action_t irq_action[IRQ_LINE_COUNT];

/**
 * irq_register_handle - registers an irq with its handler and context and
 * metadata to the irq_action table
 */
uint8_t irq_register_handle(uint8_t irq_line, irq_handler_fn_t handle,
                            void *ctx, const char *name);

/**
 * irq_unregister_handle - unregisters an handler from the irq_action table
 */
uint8_t irq_unregister_handle(uint8_t irq_line);

/**
 * irq_dispatch_line - invoker of the dispatch routine for the handler
 * @irq_line is the logical irq value
 */
irq_result_t irq_dispatch_line(uint8_t irq_line);

static inline void irq_handler(regs_t *r) {
    uint8_t irq_line = (uint8_t)(r->int_no - IRQ_BASE_VECTOR);

    irq_result_t rc = irq_dispatch_line(irq_line);

    if (rc == IRQ_NONE) {
        KLOG_WARN("IRQ", "Unhandeled irq_line %u\n", irq_line);
    }

    pic_send_eoi(irq_line);
}

#endif IRQ_H
