#ifndef TTY_STAGE_ECHO
#define TTY_STAGE_ECHO

#include "drivers/tty_pipeline.h"
#include "drivers/tty_stage.h"

/**
 * @echo_state is the state information of a stage in input pipeline.
 * echo invokes the output pipeline which prints to screen
 */
typedef struct echo_state {
    tty_pipeline_t *out_pipeline;
    tty_emit_fn out_sink;
    void *out_sink_ctx;
} echo_state_t;

tty_stage_t tty_stage_echo_make(echo_state_t *echo_state,
                                tty_pipeline_t *pipeline, tty_emit_fn emit,
                                void *emit_ctx);

#endif /* TTY_STAGE_ECHO */
