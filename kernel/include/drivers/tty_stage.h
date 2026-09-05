/**
 * @file tty_stage.h TTY submodule stage implementation for processing bytes.
 *
 * tty_write(buf) -> [STAGE 0] -> [STAGE 1] -> ... [STAGE N] -> sink(port->putc)
 */
#ifndef TTY_STAGE_H
#define TTY_STAGE_H

#include "drivers/tty_termios.h"
#include <stdint.h>

/* forward declaring so process fn can name its own def type */
struct tty_stage_def;

/* Called by a stage to push one output byte to the next stage
NOTE: emit has not information of current stage emitting. this information
should go to the context */
typedef void (*tty_emit_fn)(void *emit_ctx, uint8_t byte);

/* Called by a stage to signal the session */
typedef void (*tty_signal_fn)(void *signal_ctx, int sig);

/**
 * tty_stage_def - pure behaviour + identity. Immutable and sharable.
 * one const instance per stage lives in .rodata and is references by every
 * session. Per session STATE is delivered via the state prameter from session,
 * not stored here.
 */
typedef struct tty_stage_def {
    const char *name;

    /*
     * Process an Input byte: Transform one input byte. For each output byte
     * stage wants to send downstream it calls emits(emit_ctx, out_byte). It may
     * call emit zero, one or many times. */
    void (*process)(const struct tty_stage_def *self, uint8_t byte,
                    const ktermios_t *term, void *state, tty_emit_fn emit,
                    void *emit_ctx);
} tty_stage_def_t;

#endif /* TTY_STAGE_H */
