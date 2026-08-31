#include "drivers/tty_stage_canon.h"
#include "stddef.h"

#define ASCII_BS 0x08  /* Ctrl-H backspace */
#define ASCII_DEL 0x7F /* DEL also "backspace" on many terminals */
/* (icrnl already processed '\r' -> '\n') */
#define ASCII_LF 0x0A /* '\n' line terminator */

static void canon_echo(canon_state_t *echo_state, uint8_t byte);
static void canon_process(tty_stage_t *self, uint8_t byte,
                          const ktermios_t *term, tty_emit_fn emit,
                          void *emit_ctx);

tty_stage_t tty_stage_canon_make(canon_state_t *state, tty_pipeline_t *out_pipe,
                                 tty_emit_fn out_sink, void *out_sink_ctx) {
    state->out_pipeline = out_pipe;
    state->out_sink = out_sink;
    state->out_sink_ctx = out_sink_ctx;
    state->len = 0;

    tty_stage_t stage;
    stage.name = "canon";
    stage.process = canon_process;
    stage.state = state;

    return stage;
}

static void canon_echo(canon_state_t *state, uint8_t byte) {
    if (state->out_pipeline) {
        tty_pipeline_run(state->out_pipeline, byte, state->out_sink,
                         state->out_sink_ctx);
    } else {
        /* fallback */
        state->out_sink(state->out_sink_ctx, byte);
    }
}

static void canon_process(tty_stage_t *self, uint8_t byte,
                          const ktermios_t *term, tty_emit_fn emit,
                          void *emit_ctx) {
    canon_state_t *state = (canon_state_t *)self->state;

    /* ==== NON CANONICAL (raw) : ICANON OFF ==== */
    if (!(term->c_lflag & ICANON)) {
        /* check if echo is on */
        if (term->c_lflag & ECHO) {
            canon_echo(state, byte);
        }
        /* send downstream */
        emit(emit_ctx, byte);
        return;
    }

    /* ==== CANONICAL (cooked) : ICANON ON ==== */
    /* --- CASE A: Backspace or Delete --- */
    if (byte == ASCII_BS || byte == ASCII_DEL) {
        /* [0] Precheck - do Nothing if no characters in line buffer */
        if (state->len == 0) {
            /* no need to pass the byte to downstream */
            return;
        }

        /* [1] len > 0 => decrease line length or update the len to point to
         * prev */
        state->len--;

        /* [2] visually erase the character from the screen */
        /* echo: BS('\b'), SPACE(' '), BS('\b') */
        if (term->c_lflag & ECHO) {
            canon_echo(state, '\b');
            canon_echo(state, ' ');
            canon_echo(state, '\b');
        }

        /* [3] No need to emit the byte to downstream stages */
        return;
    }

    /* --- CASE B: Line Terminator --- */
    if (byte == ASCII_LF) {
        /* [0] Precheck - nothing - steps are safe even for empty buffer */

        /* [1] Echo the newline to the screen */
        if (term->c_lflag & ECHO) {
            canon_echo(state, byte);
        }

        /* [2] Flush (run next stages of pipeline) all the bytes in the line
         * buffer */
        for (uint32_t i = 0; i < state->len; i++) {
            /* pass to downstream stage */
            emit(emit_ctx, state->line[i]);
        }
        /* also emit current byte ASCII_LF('\n') downstream */
        emit(emit_ctx, byte);

        /* [3] update state - no need to clean buffer, len=0 takes
         * care of it */
        state->len = 0;
        return;
    }

    /* --- CASE C: Ordinary Character --- */
    {
        /* [0] Precheck - if line buffer already full - drop byte silently */
        if (state->len >= TTY_CANON_LINE_MAX) {
            /* do not print in the screen as the byte is not recorded */
            return;
        }

        /* [1] echo the byte to the screen */
        if (term->c_lflag & ECHO) {
            canon_echo(state, byte);
        }

        /* [2] store the byte in the line buffer */
        state->line[state->len++] = byte;

        /* [3] do not emit the downstream. it is held in line buffer */
        return;
    }
}
