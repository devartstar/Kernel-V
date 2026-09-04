#ifndef TTY_STAGE_CANON
#define TTY_STAGE_CANON

#include "drivers/tty_pipeline.h"

#define TTY_CANON_LINE_MAX 256

/**
 * @canon_state - defines the state for the canonical stage
 * the state matches with the echo state with a line buffer
 * idea is to keep the echo and canonical stages seperate but tightly coupled
 * CANONICAL MODE uses the canonical state (echo + line buffer)
 * eg. Press 'A' 'B' -> echo 'A' 'B' -> press 'backspace' -> echo '\b \b'
 * but in canonical state it will update the position (len) in line buffer.
 * so when user process reads line buffer it reads only 'A'
 */
typedef struct canon_state {
    uint8_t line[TTY_CANON_LINE_MAX];
    uint32_t len;

    tty_pipeline_t *out_pipeline;
    tty_emit_fn out_sink;
    void *out_sink_ctx;

    tty_signal_fn on_signal;
    void *signal_ctx;

    /* Todo: downstream: to reader (pipeline's terminal sink)
     * tty_emit_fn down_sink;
     * void *down_sink_ctx;
     */
} canon_state_t;

tty_stage_t tty_stage_canon_make(canon_state_t *state, tty_pipeline_t *out_pipe,
                                 tty_emit_fn out_sink, void *out_sink_ctx);

#endif /* TTY_STAGE_CANON */
