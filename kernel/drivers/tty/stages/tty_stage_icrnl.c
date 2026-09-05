#include "drivers/tty_stage_icrnl.h"
#include "stddef.h"

#define ASCII_LF 0x0A /* '\n' => cursor down one row */
#define ASCII_CR 0x0D /* '\r' => cursor to column 0 */

static void icrnl_process(const tty_stage_def_t *self, uint8_t byte,
                          const ktermios_t *term, void *stage_state,
                          tty_emit_fn emit, void *emit_ctx);

const tty_stage_def_t tty_stage_icrnl_def = {
    .name = "icrnl",
    .process = icrnl_process,
};

static void icrnl_process(const tty_stage_def_t *self, uint8_t byte,
                          const ktermios_t *term, void *stage_state,
                          tty_emit_fn emit, void *emit_ctx) {
    (void)self;
    (void)stage_state;

    /* Only process when input processing for CR to LF only when ICRNL flag is
     * set */
    if ((term->c_iflag & ICRNL) && byte == ASCII_CR) {
        emit(emit_ctx, ASCII_LF);
        return;
    }

    emit(emit_ctx, byte);
}
