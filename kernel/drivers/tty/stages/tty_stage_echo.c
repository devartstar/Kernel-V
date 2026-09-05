#include "drivers/tty_stage_echo.h"
#include "stddef.h"

static void echo_process(const tty_stage_def_t *self, uint8_t byte,
                         const ktermios_t *term, void *stage_state,
                         tty_emit_fn emit, void *emit_ctx) {
    (void)self;
    echo_state_t *st = (echo_state_t *)stage_state;

    /* state is null, then just forward the byte to next stage */
    if (!st) {
        emit(emit_ctx, byte);
        return;
    }

    /*
     * [1] sideways: action depends wether terminal is on raw or echo mode
     * - echo mode: pass they byte through the output pipeline processing
     * - raw mode: send the byte to the sink
     */
    if (term->c_lflag & ECHO) {
        if (st->out_pipeline) {
            tty_pipeline_run(st->out_pipeline, byte, st->out_sink,
                             st->out_sink_ctx);
        } else {
            /* raw mode fallback - send to sink without processing */
            st->out_sink(st->out_sink_ctx, byte);
        }
    }

    /* [2] downstream: forward the byte to the program (input channel) */
    emit(emit_ctx, byte);
}

const tty_stage_def_t tty_stage_echo_def = {
    .name = "echo",
    .process = echo_process,
};

void tty_stage_echo_state_init(echo_state_t *state, tty_pipeline_t *out_pipe,
                               tty_emit_fn out_sink, void *out_sink_ctx) {
    state->out_pipeline = out_pipe;
    state->out_sink = out_sink;
    state->out_sink_ctx = out_sink_ctx;
}
