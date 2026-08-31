#include "drivers/tty_stage_icrnl.h"
#include "stddef.h"

#define ASCII_LF 0x0A /* '\n' => cursor down one row */
#define ASCII_CR 0x0D /* '\r' => cursor to column 0 */

static void icrnl_process(tty_stage_t *self, uint8_t byte,
                          const ktermios_t *term, tty_emit_fn emit,
                          void *emit_ctx) {
    (void)self;

    /* Only process when input processing for CR to LF only when ICRNL flag is
     * set */
    if ((term->c_iflag & ICRNL) && byte == ASCII_CR) {
        emit(emit_ctx, ASCII_LF);
        return;
    }

    emit(emit_ctx, byte);
}

tty_stage_t tty_stage_icrnl_make() {
    tty_stage_t stage;
    stage.name = "icrml";
    stage.process = icrnl_process;
    stage.state = NULL;

    return stage;
}
