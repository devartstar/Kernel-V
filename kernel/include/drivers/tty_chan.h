#ifndef TTY_CHAN_H
#define TTY_CHAN_H

#include "proc/proc.h"
#include "sync/spinlock.h"
#include <stdint.h>

/* status code fot TTY Channels */
#define TTY_CHAN_OK 0
#define TTY_CHAN_ERR_INVALID -1
#define TTY_CHAN_ERR_FULL -2  /* no space, producer should drop or block */
#define TTY_CHAN_ERR_EMPTY -3 /* no data, consumer should block */

/**
 * tty_chan_t - one ordered byte channel.
 * A lock protected ring over a caller provided buffer.
 * A wait identity for blocking readers.
 *
 * This is just the transport primitive of TTY subsystem.
 * "local channel" = within the same system
 * "remote channel" = byte transfer across systems will wrap over this
 * interface
 */
typedef struct tty_chan {
    /* caller owned storage: channel does not allocate or free */
    uint8_t *buf;      /* backing byte buffer */
    uint32_t capacity; /* number of bytes the buffer can hold */

    /* ring cursor (indices in the buffer)
     * indices are increasing and is refernced using modulo of capacity
     *    read (r)        commit (c)         write (w)
     *		 │                 │                  │
     *		 ▼                 ▼                  ▼
     *	  ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐
     *	  │  │  │R │E │A │D │A │B │L │E │h │e │l │  │  │   ring (mod capacity)
     *	  └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘
     *		└──── committed ────┘└─ uncommitted ─┘
     *			 (consumer sees)   (producer staging)
     */
    uint32_t read;
    uint32_t commit;
    uint32_t write;

    /* for concurrency: producer runs in IRQ ctx. consumer in proc ctx */
    spinlock_t lock;

    /* optional identity token on which process will be sleeping */
    void *wait_token;
} tty_chan_t;

/**
 * tty_chan_init - bind a channel to the caller provided buffer
 * @chan - channel to initialize
 * @buf - backing storage
 * @capacity - size of buffer in bytes
 *
 * @returns TTY_CHAN_OK or TTY_CHAN_ERR_INVALID
 */
int tty_chan_init(tty_chan_t *chan, uint8_t *buf, uint32_t capacity);

/* bytes available for the consumer to read now */
static inline uint32_t tty_chan_readable(const tty_chan_t *chan) {
    return chan->commit - chan->read;
}

/* staged but un-published bytes: [commit, write) editable, reader can read yet
 */
static inline uint32_t tty_chan_staged(const tty_chan_t *chan) {
    return chan->write - chan->commit;
}

/* total occupied bytes: [commit, write) */
static inline uint32_t tty_chan_used(const tty_chan_t *chan) {
    return chan->write - chan->read;
}

/* free space available to stage new bytes */
static inline uint32_t tty_chan_free(const tty_chan_t *chan) {
    return chan->capacity - tty_chan_used(chan);
}

/**
 * tty_chan_stage - stage a byte
 * @chan - channel to stage the byte in.
 * @bye - the value to stage
 *
 * @return the status of staging
 */
int tty_chan_stage(tty_chan_t *chan, uint8_t bytes);

/**
 * tty_chan_rollback - unstage the most recent byte
 * @chan - channel to stage the byte in.
 *
 * @return the status of rollback
 */
int tty_chan_rollback(tty_chan_t *chan);

/**
 * tty_chan_commit - commit the bytes for read
 * @chan - channel to commit the byte in.
 *
 * @return number of bytes in the committed state.
 */
int tty_chan_commit(tty_chan_t *chan);

/**
 * tty_chan_read - read the committed bytes
 * @chan - channel from which to read the bytes
 * @out - buffer in which to read the bytes into.
 * @len - length of bytes to read.
 *
 * @return the number of bytes that are read.zM
 */
int tty_chan_read(tty_chan_t *chan, uint8_t *out, uint32_t len);

/**
 * tty_chan_read_blocking - block the process execution until >=1 committed
 * byte is available.
 * When bytes are available to read, read upto @len characters.
 * @chan - channel to read the bytes from
 * @out - buffer to read the bytes into
 * @len - length of the buffer to read
 * @reason - reason if any to wait on
 *
 * @return number of bytes copied or negative for error
 */
int tty_chan_read_blocking(tty_chan_t *chan, uint8_t *out, uint32_t len,
                           proc_wait_reason_t reason);

#endif /* TTY_CHAN_H */
