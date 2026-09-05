#ifndef TTY_PIPELINE_H
#define TTY_PIPELINE_H

#include "drivers/tty_stage.h"
#include "drivers/tty_termios.h"

/* maximum number of stages per pipeline */
#define TTY_STAGE_MAX 4

/**
 * tty_pipeline_def - a SHARED recipe: an ordered list of reference to stage
 * defs. Immunable, lives in .rodata, references by session. Carries no
 * per-session state
 */
typedef struct tty_pipeline_def {
    /* array of pointers to shared defs */
    const tty_stage_def_t *const *stages;
    uint32_t count;
} tty_pipeline_def_t;

/**
 * tty_pipeline - a PER-SESSION binding of shared recipe to private STATE
 * @def - an ordered array of ref. tty stages. It does not include sink.
 * @state - array of state per tty stages. NULL for stateless stage.
 * @term - reference to the session owned tty settings.
 */
typedef struct tty_pipeline {
    const tty_pipeline_def_t *def;
    void *state[TTY_STAGE_MAX];
    ktermios_t *term;
} tty_pipeline_t;

/**
 * @tty_hop - this is the tty stage process emit context. this delibrately has
 * the exact signature of @tty_emit_ctx
 * @pipeline - ref. to the tty pipenine
 * @stage_index - which tty stage is running currently.
 * @sink - sink for the terminal to emit. (SSI can enqueue to sink)
 * @sink_ctx - context for the sink
 * @term - settings for terminal session, value snapshoted before pipeline runs
 * so update in settings mid pipeline doesn't affect stage runs corrupting
 * pipeline.
 */
typedef struct tty_hop {
    tty_pipeline_t *pipeline;
    uint32_t current_stage_index;
    tty_emit_fn sink;
    void *sink_ctx;
    const ktermios_t *term;
} tty_hop_t;

void tty_pipeline_run(tty_pipeline_t *pipeline, uint8_t byte, tty_emit_fn sink,
                      void *sink_ctx);

void tty_hop(void *ctx, uint8_t byte);

#endif /* TTY_PIPELINE_H */
