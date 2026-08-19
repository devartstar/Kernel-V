#include "drivers/tty_pipeline.h"

void tty_pipeline_run(tty_pipeline_t *pipeline, uint8_t byte, tty_emit_fn sink,
                      void *sink_ctx) {
    /* set up the start context in the pipeline */
    tty_hop_t start = {.pipeline = pipeline,
                       .current_stage_index = 0,
                       .sink = sink,
                       .sink_ctx = sink_ctx};

    /* invoke the first stage of the pipeline */
    tty_hop(&start, byte);
}

void tty_hop(void *ctx, uint8_t byte) {
    tty_hop_t *stage_ctx = (tty_hop_t *)ctx;

    /* if cuurent stage index is greater than max stage in pipeline */
    if (stage_ctx->current_stage_index >= stage_ctx->pipeline->count) {
        stage_ctx->sink(stage_ctx->sink_ctx, byte);
        return;
    }

    /* here i am assuming first hop is first stage */
    uint32_t current_stage_index = stage_ctx->current_stage_index;
    tty_stage_t *current_stage =
        &stage_ctx->pipeline->stages[current_stage_index];

    uint32_t next_stage_index = current_stage_index + 1;
    tty_hop_t next_stage_ctx = {.current_stage_index = next_stage_index,
                                .pipeline = stage_ctx->pipeline,
                                .sink = stage_ctx->sink,
                                .sink_ctx = stage_ctx->sink_ctx};

    current_stage->process(current_stage, byte, tty_hop, &next_stage_ctx);
}
