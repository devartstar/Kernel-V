#include "tests/test_irq.h"

irq_result_t test_irq_handler(void *ctx) {
    KLOG_VERBOSE("TEST", "IRQ BASIC TEST HANDLER invoked.\n");
    int *counter = (int *)ctx;
    (*counter)++;
    return IRQ_HANDLED;
}

uint8_t test_irq_registration_basic(uint8_t irq_line) {
    int counter = 0;

    /* register the input irq line to the test handler */
    if (!irq_register_handle(irq_line, test_irq_handler, &counter,
                             "test_irq")) {
        KLOG_ERROR("TEST",
                   "TEST_IRQ_REGISTRATION_BASIC failed to register test "
                   "handler to the irq line %u.\n",
                   irq_line);
        return 0;
    }

    /* Verify the invocation and result from dispatch handler */
    irq_result_t res_handeled = irq_dispatch_line(irq_line);
    KLOG_VERBOSE("TEST", "TEST_IRQ_REGISTRATION_BASIC handler response %u.\n",
                 res_handeled);
    if (res_handeled != IRQ_HANDLED) {
        KLOG_ERROR("TEST",
                   "TEST_IRQ_REGISTRATION_BASIC failed to dispatch handler at "
                   "0x%08x for the test irq line %u.\n",
                   irq_action[irq_line].handler, irq_line);
    }

    /* Verify the correct exection of the irq handler */
    if (counter != 1) {
        KLOG_ERROR("TEST",
                   "TEST_IRQ_REGISTRATION_BASIC handler at 0x%08x did not "
                   "execute properly.\n",
                   irq_action[irq_line].handler);
        return 0;
    }

    /* Unregister the handler from irq_line */
    if (!irq_unregister_handle(irq_line)) {
        KLOG_ERROR("TEST",
                   "TEST_IRQ_REGISTRATION_BASIC failed unregistering test "
                   "handler for irq line %u.\n",
                   irq_line);
        return 0;
    }

    /* Dispatch after unregsitering the handler should be unhandled */
    irq_result_t res_unhandled = irq_dispatch_line(irq_line);
    if (res_unhandled != IRQ_NONE) {
        KLOG_ERROR("TEST",
                   "TEST_IRQ_REGISTRATION_BASIC dispatch succeeded after "
                   "unregistering the handler for irq line %u.\n",
                   irq_line);
        return 0;
    }

    return 1;
}

irq_result_t dummy_irq_handler(void *ctx) {
    KLOG_VERBOSE("TEST", "IRQ TEST DUMMY HANDLER invoked.\n");
    (void)ctx;
    return IRQ_HANDLED;
}

uint8_t test_irq_duplicate_registration(uint8_t irq_line) {
    /* register the first handler to the given irq line */
    if (!irq_register_handle(irq_line, dummy_irq_handler, NULL,
                             "irq_test_first")) {
        KLOG_ERROR("TEST",
                   "IRQ_DUPLICATE_REGISTRATION failed to register the first "
                   "handler to irq line %u.\n",
                   irq_line);
        return 0;
    }

    /* register a duplicate handler to the same irq line (should fail) */
    if (irq_register_handle(irq_line, dummy_irq_handler, NULL,
                            "irq_test_second")) {
        KLOG_ERROR("TEST",
                   "IRQ_DUPLICATE_REGISTRATION duplicate registration "
                   "unexpectedly succeeded to irq line %u.\n",
                   irq_line);
        return 0;
    }

    /* unregister the handler from the irq line */
    if (!irq_unregister_handle(irq_line)) {
        KLOG_ERROR("TEST",
                   "IRQ_DUPLICATE_REGISTRATION unregistering handle failed for "
                   "irq line %u.\n",
                   irq_line);
        return 0;
    }

    return 1;
}
