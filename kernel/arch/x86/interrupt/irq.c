#include "arch/x86/irq.h"
#include "lib/printk.h"

uint8_t irq_register_handle(uint8_t irq_line, irq_handler_fn_t handler,
                            void *ctx, const char *name) {
    /* Fail if irq line is out of bounds */
    if (irq_line >= IRQ_LINE_COUNT) {
        KLOG_ERROR("IRQ", "Failed to register handler for irq_line %u.\n",
                   irq_line);
        return 0;
    }

    /* Fail if null pointer to the irq handler */
    if (!handler) {
        KLOG_ERROR("IRQ", "Failed to register null handler to irq_line %u.\n",
                   irq_line);
        return 0;
    }

    /* enter a critical section */
    irq_flags_t flags = irq_save();

    /* Check if handler is already registerd for the given irq */
    if (irq_action[irq_line].registered) {
        KLOG_ERROR("IRQ", "Handler already registered for irq_line %u.\n");
        irq_restore(flags);
        return 0;
    }

    /* Initialize the irq_action table entry from giver irq_line */
    irq_action[irq_line].handler = handler;
    irq_action[irq_line].ctx = ctx;
    irq_action[irq_line].name = name;
    irq_action[irq_line].registered = 1;

    irq_restore(flags);
    return 1;
}

uint8_t irq_unregister_handle(uint8_t irq_line) {
    /* Fail if irq line is out of bounds */
    if (irq_line >= IRQ_LINE_COUNT) {
        KLOG_ERROR("IRQ", "Failed to register handler for irq_line %u.\n",
                   irq_line);
        return 0;
    }

    /* enter critical section */
    irq_flags_t flags = irq_save();

    /* If irq is not registered with a handler */
    if (!irq_action[irq_line].registered) {
        KLOG_ERROR("IRQ", "No active registration for irq_line %u.\n",
                   irq_line);
        return 0;
    }

    /* unregister the entry from irq_action table */
    irq_action[irq_line].handler = NULL;
    irq_action[irq_line].ctx = NULL;
    irq_action[irq_line].name = NULL;
    irq_action[irq_line].registered = NULL;

    irq_restore(flags);
    return 1;
}

irq_result_t irq_dispatch_line(uint8_t irq_line) {
    /* Check if the irq to dispatch is within bounds */
    if (irq_line >= IRQ_LINE_COUNT) {
        KLOG_ERROR("IRQ", "Failed to dispatch irq_line %u.\n", irq_line);
        return 0;
    }

    irq_action_t *action = &irq_action[irq_line];

    if (!action->registered || !action->handler) {
        KLOG_ERROR("IRQ",
                   "Error dispatching for irq_line %u. Invalid or unregistered "
                   "handler.\n",
                   irq_line);
        return 0;
    }

    return action->handler(action->ctx);
}
