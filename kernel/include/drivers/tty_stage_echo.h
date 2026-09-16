#ifndef TTY_STAGE_ECHO
#define TTY_STAGE_ECHO

#include "drivers/tty_pipeline.h"
#include "drivers/tty_stage.h"

extern const tty_stage_def_t tty_stage_echo_def;

/**
 * @echo_state is the state information of a stage in input pipeline.
 * echo invokes the output pipeline which prints to screen
 */
typedef struct echo_state {
    tty_pipeline_t *out_pipeline;
    tty_emit_fn out_sink;
    void *out_sink_ctx;
} echo_state_t;

void tty_stage_echo_state_init(echo_state_t *state, tty_pipeline_t *out_pipe,
                               tty_emit_fn out_sink, void *out_sink_ctx);

#endif /* TTY_STAGE_ECHO */
