#include "drivers/tty_pipeline.h"

void tty_pipeline_run(tty_pipeline_t *pipeline, uint8_t byte, tty_emit_fn sink,
                      void *sink_ctx) {
    /* set up the start context in the pipeline */
    tty_hop_t start;
    start.pipeline = pipeline;
    start.current_stage_index = 0;
    start.sink = sink;
    start.sink_ctx = sink_ctx;

    /* snapshot the terminal config ONCE for this run
     * All stages read the snapshoted value, so a concurrent
     * set_termios cannot produce a torn view mid-character
     */
    ktermios_t snapshot;
    if (pipeline->term) {
        /* copy value once */
        snapshot = *pipeline->term;
    } else {
        /* no config - all features off */
        snapshot.c_iflag = 0;
        snapshot.c_oflag = 0;
        snapshot.c_lflag = 0;
    }

    /* assign reference to the snapshotted value */
    start.term = &snapshot;

    /* invoke the first stage of the pipeline */
    tty_hop(&start, byte);
}

void tty_hop(void *ctx, uint8_t byte) {
    tty_hop_t *hop = (tty_hop_t *)ctx;
    tty_pipeline_t *pipeline = hop->pipeline;
    const tty_pipeline_def_t *def = pipeline->def;

    uint32_t current_stage_index = hop->current_stage_index;

    /* past the last stage (or empty pipeline) -> sink */
    if (!def || current_stage_index >= def->count) {
        hop->sink(hop->sink_ctx, byte);
        return;
    }

    /* CURRENT STAGE */
    const tty_stage_def_t *current_stage = def->stages[current_stage_index];
    void *current_stage_state = pipeline->state[current_stage_index];

    /* NEXT STAGE */
    uint32_t next_stage_index = current_stage_index + 1;
    tty_hop_t next_stage_ctx = {.current_stage_index = next_stage_index,
                                .pipeline = pipeline,
                                .sink = hop->sink,
                                .sink_ctx = hop->sink_ctx,
                                .term = hop->term};

    current_stage->process(current_stage, byte, hop->term, current_stage_state,
                           tty_hop, &next_stage_ctx);
}
