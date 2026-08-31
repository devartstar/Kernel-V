/**
 * @file test_tty_pipeline.c  Table-driven tests for the TTY line-discipline
 * pipelines.
 *
 *  - OUTPUT cases : run through the output discipline [onlcr] (or empty).
 *  - INPUT  cases : run through [icrnl, <mode stage>]; EACH case is executed in
 *                   BOTH modes -> RAW (echo stage) and CANON (canonical stage).
 *
 * Two destinations are verified for input cases:
 *   downstream (cap_down) -> what a reader / program would receive
 *   echo       (cap_echo) -> what appears on screen (echoed via [onlcr])
 */

#include "tests/test_tty_pipeline.h"
#include "drivers/tty_pipeline.h"
#include "drivers/tty_stage.h"
#include "drivers/tty_stage_canon.h"
#include "drivers/tty_stage_echo.h"
#include "drivers/tty_stage_icrnl.h"
#include "drivers/tty_stage_onlcr.h"

typedef enum mode_type { MODE_RAW, MODE_CANON } mode_type_t;

typedef struct capture {
    uint8_t buf[64];
    uint32_t len;
} capture_t;

/* an expected byte sequence (bytes may be NULL when len == 0) */
typedef struct expected {
    const uint8_t *bytes;
    uint32_t len;
} expected_t;

/* an OUTPUT-pipeline test case (pure data) */
typedef struct out_case {
    const char *name;
    const uint8_t *in;
    uint32_t in_len;
    const uint8_t *exp;
    uint32_t exp_len;
    uint32_t stage_count; /* 0 = empty passthrough, 1 = [onlcr] */
} out_case_t;

/* an INPUT-pipeline test case: same input, per-mode expectations */
typedef struct in_case {
    const char *name;
    const uint8_t *in;
    uint32_t in_len;
    expected_t raw_down;   /* RAW mode: downstream (reader) */
    expected_t raw_echo;   /* RAW mode: screen echo         */
    expected_t canon_down; /* CANON mode: downstream        */
    expected_t canon_echo; /* CANON mode: screen echo       */
} in_case_t;

/* build the termios structure for the different modes */
static void termios_for_mode(ktermios_t *term, mode_type_t mode) {
    term->c_iflag = ICRNL;
    term->c_oflag = OPOST | ONLCR;
    if (mode == MODE_RAW) {
        term->c_lflag = ECHO;
    } else if (mode == MODE_CANON) {
        term->c_lflag = ICANON | ECHO;
    }
}

/* simulating tty_port_sink locally: instead of emitting the byte, capture it */
static void capture_sink(void *ctx, uint8_t byte) {
    capture_t *cap = (capture_t *)ctx;
    if (cap->len < sizeof(cap->buf)) {
        cap->buf[cap->len++] = byte;
    }
}

/* compare a captured buffer against an expected sequence */
static uint8_t cap_verify(const char *name, const char *mode, const char *label,
                          const capture_t *got, const uint8_t *exp,
                          uint32_t exp_len) {
    if (got->len != exp_len) {
        KLOG_ERROR("TTY_PIPELINE",
                   "[%s/%s] %s len mismatch: got=%u expected=%u.\n", name, mode,
                   label, got->len, exp_len);
        return 0;
    }
    for (uint32_t i = 0; i < exp_len; i++) {
        if (got->buf[i] != exp[i]) {
            KLOG_ERROR("TTY_PIPELINE",
                       "[%s/%s] %s byte %u mismatch: got=0x%x expected=0x%x.\n",
                       name, mode, label, i, got->buf[i], exp[i]);
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/*  OUTPUT pipeline runner                                            */
/* ------------------------------------------------------------------ */
static uint8_t run_output_case(const out_case_t *c) {
    tty_stage_t stages[1];
    tty_pipeline_t pipe;

    ktermios_t term;
    termios_for_mode(&term, MODE_CANON);

    stages[0] = tty_stage_onlcr_make();
    pipe.stages = stages;
    pipe.count = c->stage_count; /* 0 -> empty passthrough, 1 -> onlcr */
    pipe.term = &term;

    capture_t cap = {.len = 0};
    for (uint32_t i = 0; i < c->in_len; i++) {
        tty_pipeline_run(&pipe, c->in[i], capture_sink, &cap);
    }

    if (!cap_verify(c->name, "out", "downstream", &cap, c->exp, c->exp_len)) {
        return 0;
    }

    KLOG_INFO("TTY_PIPELINE", "[%s/out] passed.\n", c->name);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  INPUT pipeline runner: [icrnl, <mode stage>], one mode per call   */
/* ------------------------------------------------------------------ */
static uint8_t run_input_case_mode(const in_case_t *c, mode_type_t mode) {
    const char *mode_name = (mode == MODE_RAW) ? "raw" : "canon";

    ktermios_t term;
    termios_for_mode(&term, mode);

    /* [1] output pipeline [onlcr] -> cap_echo (the "screen") */
    tty_stage_t out_stages[1];
    tty_pipeline_t out_pipe;
    out_stages[0] = tty_stage_onlcr_make();
    out_pipe.stages = out_stages;
    out_pipe.count = 1;
    out_pipe.term = &term;
    capture_t cap_echo = {.len = 0};

    /* [2] build the mode-specific second stage, echoing through out_pipe.
     *     both state structs are function-scoped: alive for the whole run. */
    canon_state_t canon_state;
    tty_stage_t mode_stage =
        tty_stage_canon_make(&canon_state, &out_pipe, capture_sink, &cap_echo);

    /* [3] input pipeline [icrnl, mode_stage] -> cap_down (the "reader") */
    tty_stage_t in_stages[2];
    tty_pipeline_t in_pipe;
    in_stages[0] = tty_stage_icrnl_make();
    in_stages[1] = mode_stage;
    in_pipe.stages = in_stages;
    in_pipe.count = 2;
    in_pipe.term = &term;
    capture_t cap_down = {.len = 0};

    /* [4] Initialize the expected bytes for the different modes */
    const expected_t *exp_down =
        (mode == MODE_RAW) ? &c->raw_down : &c->canon_down;
    const expected_t *exp_echo =
        (mode == MODE_RAW) ? &c->raw_echo : &c->canon_echo;

    /* [5] drive every input byte through the pipeline */
    for (uint32_t i = 0; i < c->in_len; i++) {
        tty_pipeline_run(&in_pipe, c->in[i], capture_sink, &cap_down);
    }

    /* [6] verify BOTH destinations against the mode-specific expectations */
    if (!cap_verify(c->name, mode_name, "downstream", &cap_down,
                    exp_down->bytes, exp_down->len)) {
        return 0;
    }
    if (!cap_verify(c->name, mode_name, "echo", &cap_echo, exp_echo->bytes,
                    exp_echo->len)) {
        return 0;
    }

    KLOG_INFO("TTY_PIPELINE", "[%s/%s] passed.\n", c->name, mode_name);
    return 1;
}

/* ================================================================== */
/*  OUTPUT CASES (data)                                               */
/* ================================================================== */
static const uint8_t o1_in[] = {'a', '\n', 'b', '\n'};
static const uint8_t o1_exp[] = {'a', '\r', '\n', 'b', '\r', '\n'};

static const uint8_t o2_in[] = {'a', 'b', 'c'};
static const uint8_t o2_exp[] = {'a', 'b', 'c'};

static const uint8_t o3_in[] = {'\n'};
static const uint8_t o3_exp[] = {'\r', '\n'};

static const uint8_t o4_in[] = {'x', '\n', 'y'};
static const uint8_t o4_exp[] = {'x', '\n', 'y'}; /* empty pipe -> verbatim */

static const out_case_t g_out_cases[] = {
    {"newline", o1_in, sizeof(o1_in), o1_exp, sizeof(o1_exp), 1},
    {"no_newline", o2_in, sizeof(o2_in), o2_exp, sizeof(o2_exp), 1},
    {"standalone_newline", o3_in, sizeof(o3_in), o3_exp, sizeof(o3_exp), 1},
    {"empty_passthrough", o4_in, sizeof(o4_in), o4_exp, sizeof(o4_exp), 0},
};

/* ================================================================== */
/*  INPUT CASES (data) -- each runs in BOTH raw and canon             */
/*  NOTE: icrnl converts '\r' -> '\n' BEFORE the mode stage.          */
/* ================================================================== */

/* A: simple line "hi<Enter>". Raw and canon agree (no editing). */
static const uint8_t a_in[] = {'h', 'i', '\r'};
static const uint8_t a_raw_down[] = {'h', 'i', '\n'};
static const uint8_t a_raw_echo[] = {'h', 'i', '\r', '\n'};
static const uint8_t a_can_down[] = {'h', 'i', '\n'};
static const uint8_t a_can_echo[] = {'h', 'i', '\r', '\n'};

/* B: backspace mid-line "ab<BS>c<Enter>". This is where modes DIVERGE:
 *    raw forwards the raw 0x08 downstream; canon erases 'b' from the line. */
static const uint8_t b_in[] = {'a', 'b', 0x08, 'c', '\r'};
static const uint8_t b_raw_down[] = {'a', 'b', 0x08, 'c', '\n'};
static const uint8_t b_raw_echo[] = {'a', 'b', 0x08, 'c', '\r', '\n'};
static const uint8_t b_can_down[] = {'a', 'c', '\n'}; /* 'b' erased */
static const uint8_t b_can_echo[] = {'a',  'b', 0x08, ' ',
                                     0x08, 'c', '\r', '\n'}; /* \b \b erase */

/* C: backspace on empty buffer "<BS><BS>x<Enter>".
 *    canon must NO-OP the empty backspaces (no erase, no downstream). */
static const uint8_t c_in[] = {0x08, 0x08, 'x', '\r'};
static const uint8_t c_raw_down[] = {0x08, 0x08, 'x', '\n'};
static const uint8_t c_raw_echo[] = {0x08, 0x08, 'x', '\r', '\n'};
static const uint8_t c_can_down[] = {'x', '\n'};
static const uint8_t c_can_echo[] = {'x', '\r', '\n'}; /* no stray \b \b */

/* D: no Enter "ab". Proves canon HOLDS (0 downstream) while raw forwards. */
static const uint8_t d_in[] = {'a', 'b'};
static const uint8_t d_raw_down[] = {'a', 'b'};
static const uint8_t d_raw_echo[] = {'a', 'b'};
/* canon_down is empty (len 0) */
static const uint8_t d_can_echo[] = {'a', 'b'};

/* E: bare Enter "<Enter>". Empty line still terminates. */
static const uint8_t e_in[] = {'\r'};
static const uint8_t e_raw_down[] = {'\n'};
static const uint8_t e_raw_echo[] = {'\r', '\n'};
static const uint8_t e_can_down[] = {'\n'};
static const uint8_t e_can_echo[] = {'\r', '\n'};

/* F: DEL (0x7F) key "a<DEL><Enter>". canon treats DEL == BS; raw forwards it.
 */
static const uint8_t f_in[] = {'a', 0x7F, '\r'};
static const uint8_t f_raw_down[] = {'a', 0x7F, '\n'};
static const uint8_t f_raw_echo[] = {'a', 0x7F, '\r', '\n'};
static const uint8_t f_can_down[] = {'\n'}; /* 'a' erased by DEL */
static const uint8_t f_can_echo[] = {'a', 0x08, ' ', 0x08, '\r', '\n'};

static const in_case_t g_in_cases[] = {
    {"line",
     a_in,
     sizeof(a_in),
     {a_raw_down, sizeof(a_raw_down)},
     {a_raw_echo, sizeof(a_raw_echo)},
     {a_can_down, sizeof(a_can_down)},
     {a_can_echo, sizeof(a_can_echo)}},

    {"backspace",
     b_in,
     sizeof(b_in),
     {b_raw_down, sizeof(b_raw_down)},
     {b_raw_echo, sizeof(b_raw_echo)},
     {b_can_down, sizeof(b_can_down)},
     {b_can_echo, sizeof(b_can_echo)}},

    {"empty_backspace",
     c_in,
     sizeof(c_in),
     {c_raw_down, sizeof(c_raw_down)},
     {c_raw_echo, sizeof(c_raw_echo)},
     {c_can_down, sizeof(c_can_down)},
     {c_can_echo, sizeof(c_can_echo)}},

    {"hold_until_enter",
     d_in,
     sizeof(d_in),
     {d_raw_down, sizeof(d_raw_down)},
     {d_raw_echo, sizeof(d_raw_echo)},
     {NULL, 0},
     {d_can_echo, sizeof(d_can_echo)}}, /* canon_down empty */

    {"bare_enter",
     e_in,
     sizeof(e_in),
     {e_raw_down, sizeof(e_raw_down)},
     {e_raw_echo, sizeof(e_raw_echo)},
     {e_can_down, sizeof(e_can_down)},
     {e_can_echo, sizeof(e_can_echo)}},

    {"del_key",
     f_in,
     sizeof(f_in),
     {f_raw_down, sizeof(f_raw_down)},
     {f_raw_echo, sizeof(f_raw_echo)},
     {f_can_down, sizeof(f_can_down)},
     {f_can_echo, sizeof(f_can_echo)}},
};

/* ================================================================== */
/*  Public invokers                                                   */
/* ================================================================== */
uint32_t tty_run_output_pipeline_cases(void) {
    uint32_t failed = 0;
    uint32_t n = sizeof(g_out_cases) / sizeof(g_out_cases[0]);
    for (uint32_t i = 0; i < n; i++) {
        if (!run_output_case(&g_out_cases[i])) {
            failed++;
        }
    }
    return failed;
}

uint32_t tty_run_input_pipeline_cases(void) {
    uint32_t failed = 0;
    uint32_t n = sizeof(g_in_cases) / sizeof(g_in_cases[0]);
    for (uint32_t i = 0; i < n; i++) {
        /* run EACH case in both modes */
        if (!run_input_case_mode(&g_in_cases[i], MODE_RAW)) {
            failed++;
        }
        if (!run_input_case_mode(&g_in_cases[i], MODE_CANON)) {
            failed++;
        }
    }
    return failed;
}
