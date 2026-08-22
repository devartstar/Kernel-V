#include "tests/test_tty_pipeline.h"
#include "drivers/tty_pipeline.h"
#include "drivers/tty_stage.h"
#include "drivers/tty_stage_echo.h"
#include "drivers/tty_stage_icrnl.h"
#include "drivers/tty_stage_onlcr.h"

typedef struct capture {
    uint8_t buf[64];
    uint32_t len;
} capture_t;

typedef struct echo_capture {
    capture_t got_echo_buf;
    tty_stage_t echo_stage;
    capture_t exp_echo_buf;
} echo_capture_t;

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

static uint8_t run_icrnl_case(const char *case_name, const uint8_t *in,
                              uint32_t in_len, const uint8_t *exp_downstream,
                              uint32_t exp_downstream_len,
                              echo_capture_t *echo_info) {
    tty_stage_t in_stages[2];
    tty_pipeline_t in_pipe;

    /* initialize the input pipeline with echo */
    in_stages[0] = tty_stage_icrnl_make();
    in_pipe.count = 1;
    if (echo_info) {
        in_stages[in_pipe.count] = echo_info->echo_stage;
        in_pipe.count++;
    }
    in_pipe.stages = in_stages;

    capture_t cap_down = {.len = 0};

    /* run the pipeline for every byte */
    for (uint32_t i = 0; i < in_len; i++) {
        tty_pipeline_run(&in_pipe, in[i], capture_sink, &cap_down);
    }

    /* verify the expected byte count post processing */
    if (cap_down.len != exp_downstream_len) {
        KLOG_ERROR("TTY_PIPELINE",
                   "[%s] downstream len mismatch: got=%u expected=%u.\n",
                   case_name, cap_down.len, exp_downstream_len);
        return 0;
    }

    /* verify the processed downstreamed bytes with expected bytes */
    for (uint32_t i = 0; i < exp_downstream_len; i++) {
        if (cap_down.buf[i] != exp_downstream[i]) {
            KLOG_ERROR(
                "TTY_PIPELINE",
                "[%s] downstream byte %u mismatch: got=0x%x expected=0x%x.\n",
                case_name, i, cap_down.buf[i], exp_downstream[i]);
            return 0;
        }
    }

    /* verify the echoed bytes */
    if (echo_info) {
        /* verify the echoed length with expected echo length */
        if (echo_info->got_echo_buf.len != echo_info->exp_echo_buf.len) {
            KLOG_ERROR("TTY_PIPELINE",
                       "[%s] echo len mismatch: got=%u expected=%u.\n",
                       case_name, echo_info->got_echo_buf.len,
                       echo_info->exp_echo_buf.len);
            return 0;
        }

        /* verify for exact echo bytes with the expeacted echoed bytes */
        for (uint32_t i = 0; i < echo_info->exp_echo_buf.len; i++) {
            if (echo_info->got_echo_buf.buf[i] !=
                echo_info->exp_echo_buf.buf[i]) {
                KLOG_ERROR(
                    "TTY_PIPELINE",
                    "[%s] echo byte %u mismatch: got=0x%x expected=0x%x.\n",
                    case_name, i, echo_info->got_echo_buf.buf[i],
                    echo_info->exp_echo_buf.buf[i]);
                return 0;
            }
        }
    }

    KLOG_INFO("TTY_PIPELINE", "[%s] passed.\n", case_name);
    return 1;
}

/** *** OUTPUT PIPELINE CASES *** */

/* Case 1: input with new line is perocessed. */
uint8_t tty_out_pipeline_newline() {
    const uint8_t in[] = {'a', '\n', 'b', '\n'};
    const uint8_t exp[] = {'a', '\r', '\n', 'b', '\r', '\n'};

    return run_onlcr_case("newline", in, sizeof(in), exp, sizeof(exp));
}

/* Case 2: input with NO newline is passed through untouched. */
uint8_t tty_out_pipeline_no_newline() {
    const uint8_t in[] = {'a', 'b', 'c'};
    const uint8_t exp[] = {'a', 'b', 'c'};
    return run_onlcr_case("no_newline", in, sizeof(in), exp, sizeof(exp));
}

/* Case 3: a standalone leading '\n' expands to "\r\n" at position 0. */
uint8_t tty_out_pipeline_standalone_newline() {
    const uint8_t in[] = {'\n'};
    const uint8_t exp[] = {'\r', '\n'};
    return run_onlcr_case("standalone_newline", in, sizeof(in), exp,
                          sizeof(exp));
}

/* Case 4: empty pipeline (count == 0) is a clean pass-through -> proves the
 * base case `index >= count` routes straight to the sink (raw-mode semantics).
 */
uint8_t tty_out_pipeline_empty_passthrough() {
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

/** *** INPUT PIPELINE CASES *** */

/* Case 1: input with new line processes */
uint8_t tty_in_pipeline_newline() {
    echo_state_t echo_state;

    echo_capture_t echo_info;
    echo_info.got_echo_buf.len = 0;

    /* create echo stage with null out pipeline - default to capture_sink */
    echo_info.echo_stage = tty_stage_echo_make(&echo_state, NULL, capture_sink,
                                               &echo_info.got_echo_buf);

    const uint8_t in[] = {'a', '\r'};
    const uint8_t exp[] = {'a', '\n'}; /* icrnl: converts \r -> \n */
    echo_info.exp_echo_buf = (capture_t){
        .buf = {'a', '\n'},
        .len = 2,
    };

    return run_icrnl_case("in_newline", in, sizeof(in), exp, sizeof(exp),
                          &echo_info);
}
