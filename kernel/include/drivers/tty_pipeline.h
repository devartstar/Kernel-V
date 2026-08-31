#ifndef TTY_PIPELINE_H
#define TTY_PIPELINE_H

#include "drivers/tty_stage.h"
#include "drivers/tty_termios.h"

/**
 * @tty_pipeline - an ordered array of tty stages. It does not include sink.
 * @stages - array of tty stages.
 * @count - number of tty stages
 * @term - reference to the session owned tty settings.
 */
typedef struct tty_pipeline {
    tty_stage_t *stages;
    uint32_t count;
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
