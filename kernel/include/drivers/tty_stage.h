/**
 * @file tty_stage.h TTY submodule stage implementation for processing bytes.
 *
 * tty_write(buf) -> [STAGE 0] -> [STAGE 1] -> ... [STAGE N] -> sink(port->putc)
 */
#ifndef TTY_STAGE_H
#define TTY_STAGE_H

#include <stdint.h>

/* Called by a stage to push one output byte to the next stage
NOTE: emit has not information of current stage emitting. this information
should go to the context */
typedef void (*tty_emit_fn)(void *emit_ctx, uint8_t byte);

/**
 * tty_stage - structure defining a tty stage.
 * a tty stage transforms a bytestream.
 * It takes in 1 byte and emits 0..N bytes downstream to next stage.
 *
 * @name - name of the current stage
 * @process - ref. to a routine which will operate on the byte.
 */
typedef struct tty_stage {
    const char *name;

    /*
     * Process an Input byte: Transform one input byte. For each output byte
     * stage wants to send downstream it calls emits(emit_ctx, out_byte). It may
     * call emit zero, one or many times. */
    void (*process)(struct tty_stage *self, uint8_t byte, tty_emit_fn emit,
                    void *emit_ctx);

    /* optional per stage state. For stateless stages should be NULL */
    void *state;
} tty_stage_t;

#endif /* TTY_STAGE_H */
