#include "tests/test_tty_pipeline.h"
#include "drivers/tty_pipeline.h"
#include "drivers/tty_stage.h"
#include "drivers/tty_stage_onlcr.h"

typedef struct capture {
    uint8_t buf[64];
    uint32_t len;
} capture_t;

/* simulating the tty_port_sink locally.
 * instead of emiting the byte just capture it */
static void capture_sink(void *ctx, uint8_t byte) {
    capture_t *cap = (capture_t *)ctx;
    if (cap->len < sizeof(cap->buf)) {
        cap->buf[cap->len++] = byte;
    }
}

/**
 * Run a following case on a fresh pipeline
 */
static uint8_t run_onlcr_case(const char *case_name, const uint8_t *in,
                              uint32_t in_len, const uint8_t *expected,
                              uint32_t exp_len) {
    tty_stage_t stages[1];
    tty_pipeline_t pipe;

    /* setting up line discipline pipeline */
    stages[0] = tty_stage_onlcr_make();
    pipe.stages = stages;
    pipe.count = 1;

    /* initialize ctx and bytes and process thru line discipline */
    capture_t cap = {.len = 0};
    for (uint32_t i = 0; i < in_len; i++) {
        tty_pipeline_run(&pipe, in[i], capture_sink, &cap);
    }

    /* initlialize expected bytes post processing */
    if (cap.len != exp_len) {
        KLOG_ERROR("TTY_PIPELINE", "[%s] len mismatch: got=%u expected=%u.\n",
                   case_name, cap.len, exp_len);
        return 0;
    }

    for (uint32_t i = 0; i < exp_len; i++) {
        if (cap.buf[i] != expected[i]) {
            KLOG_ERROR("TTY_PIPELINE",
                       "[%s] byte %u mismatch: got=0x%x expected=0x%x.\n",
                       case_name, i, cap.buf[i], expected[i]);
            return 0;
        }
    }

    KLOG_INFO("TTY_PIPELINE", "[%s] passed.\n", case_name);
    return 1;
}

/* Case 1: input with new line is perocessed. */
uint8_t tty_pipeline_newline() {
    const uint8_t in[] = {'a', '\n', 'b', '\n'};
    const uint8_t exp[] = {'a', '\r', '\n', 'b', '\r', '\n'};

    return run_onlcr_case("newline", in, sizeof(in), exp, sizeof(exp));
}

/* Case 2: input with NO newline is passed through untouched. */
uint8_t tty_pipeline_no_newline() {
    const uint8_t in[] = {'a', 'b', 'c'};
    const uint8_t exp[] = {'a', 'b', 'c'};
    return run_onlcr_case("no_newline", in, sizeof(in), exp, sizeof(exp));
}

/* Case 3: a standalone leading '\n' expands to "\r\n" at position 0. */
uint8_t tty_pipeline_standalone_newline() {
    const uint8_t in[] = {'\n'};
    const uint8_t exp[] = {'\r', '\n'};
    return run_onlcr_case("standalone_newline", in, sizeof(in), exp,
                          sizeof(exp));
}

/* Case 4: empty pipeline (count == 0) is a clean pass-through -> proves the
 * base case `index >= count` routes straight to the sink (raw-mode semantics).
 */
uint8_t tty_pipeline_empty_passthrough() {
    tty_stage_t stages[1]; /* unused, count is 0 */
    tty_pipeline_t pipe;
    pipe.stages = stages;
    pipe.count = 0;

    capture_t cap = {.len = 0};
    const uint8_t in[] = {'x', '\n', 'y'};
    for (uint32_t i = 0; i < sizeof(in); i++) {
        tty_pipeline_run(&pipe, in[i], capture_sink, &cap);
    }

    /* count==0 => no stage runs, no translation, bytes pass verbatim */
    const uint8_t exp[] = {'x', '\n', 'y'};
    if (cap.len != sizeof(exp)) {
        KLOG_ERROR("TTY_PIPELINE",
                   "[empty_passthrough] len mismatch: got=%u expected=%u.\n",
                   cap.len, (uint32_t)sizeof(exp));
        return 0;
    }
    for (uint32_t i = 0; i < sizeof(exp); i++) {
        if (cap.buf[i] != exp[i]) {
            KLOG_ERROR("TTY_PIPELINE",
                       "[empty_passthrough] byte %u mismatch.\n", i);
            return 0;
        }
    }
    KLOG_INFO("TTY_PIPELINE", "[empty_passthrough] passed.\n");
    return 1;
}
