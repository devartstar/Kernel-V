#include "drivers/tty_stage_echo.h"
#include "stddef.h"

static void echo_process(tty_stage_t *self, uint8_t byte,
                         const ktermios_t *term, tty_emit_fn emit,
                         void *emit_ctx) {
    echo_state_t *st = (echo_state_t *)self->state;

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

tty_stage_t tty_stage_echo_make(echo_state_t *echo_state,
                                tty_pipeline_t *pipeline, tty_emit_fn emit,
                                void *emit_ctx) {
    tty_stage_t stage;
    echo_state->out_pipeline = pipeline;
    echo_state->out_sink = emit;
    echo_state->out_sink_ctx = emit_ctx;

    stage.name = "echo";
    stage.process = echo_process;
    stage.state = (void *)echo_state;

    return stage;
}
