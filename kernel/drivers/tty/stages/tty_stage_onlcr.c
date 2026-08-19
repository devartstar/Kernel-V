#include "drivers/tty_stage.h"
#include <stddef.h>

#define ASCII_LF 0x0A /* '\n' => cursor down one row */
#define ASCII_CR 0x0D /* '\r' => cursor to column 0 */

static void onlcr_process(tty_stage_t *self, uint8_t byte, tty_emit_fn emit,
                          void *emit_ctx) {
    /* onclr stage is stateless, ie. it doesnt invoke itself with updated
     * states */
    (void)self;

    /* process the byte only if it is line-feed('\n') */
    if (byte == ASCII_LF) {
        emit(emit_ctx, ASCII_CR);
        emit(emit_ctx, ASCII_LF);
        return;
    }

    /* no processing needed - pass thru */
    emit(emit_ctx, byte);
}

tty_stage_t tty_stage_onlcr_make(void) {
    tty_stage_t stage;

    stage.name = "onlcr";
    stage.process = onlcr_process;
    stage.state = NULL;

    return stage;
}
