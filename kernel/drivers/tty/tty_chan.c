#include "drivers/tty_chan.h"
#include "proc/proc.h"

int tty_chan_init(tty_chan_t *chan, uint8_t *buf, uint32_t capacity) {
    if (!chan) {
        return TTY_CHAN_ERR_INVALID;
    }

    if (!buf) {
        return TTY_CHAN_ERR_INVALID;
    }

    if (capacity == 0) {
        return TTY_CHAN_ERR_INVALID;
    }

    chan->buf = buf;
    chan->capacity = capacity;
    chan->read = 0;
    chan->commit = 0;
    chan->write = 0;
    chan->lock = (spinlock_t)SPINLOCK_INIT;
    chan->wait_token = (void *)chan;

    return TTY_CHAN_OK;
}

int tty_chan_stage(tty_chan_t *chan, uint8_t byte) {
    irq_flags_t flags;

    /* check for valid channel */
    if (!chan) {
        return TTY_CHAN_ERR_INVALID;
    }

    /* Producer might run in IRQ ctx. Consumer in proc ctx
     * lock here - producer is staging a byte but at the same time IRQ preempts
     * a consumer(reader) mid update and both touch the same cursor position */
    flags = spin_lock_irqsave(&chan->lock);

    /*
     * Reject staging if the ring is already full.
     * Checked under lock so ref. positions cannot update */
    if (tty_chan_free(chan) == 0) {
        /* no free space in the ring */
        spin_unlock_irqrestore(&chan->lock, flags);
        return TTY_CHAN_ERR_FULL;
    }

    /* stage the byte to a physical slot */
    uint8_t idx = chan->write % chan->capacity;
    chan->buf[idx] = byte;

    /* advance the staging head only. not visible to reader yet.
     * reader can only read between commit and write */
    chan->write++;

    spin_unlock_irqrestore(&chan->lock, flags);
    return TTY_CHAN_OK;
}

int tty_chan_rollback(tty_chan_t *chan) {
    irq_flags_t flags;

    /* check for valid channel */
    if (!chan) {
        return TTY_CHAN_ERR_INVALID;
    }

    flags = spin_lock_irqsave(&chan->lock);

    if (tty_chan_staged(chan) == 0) {
        /* nothing in the buffer to rollback */
        spin_unlock_irqrestore(&chan->lock, flags);
        return TTY_CHAN_ERR_EMPTY;
    }

    /* rollback the byte in the latest slot */
    chan->write--;

    spin_unlock_irqrestore(&chan->lock, flags);
    return TTY_CHAN_OK;
}

int tty_chan_commit(tty_chan_t *chan) {
    irq_flags_t flags;
    uint32_t len_to_commit;

    /* check for valid channel */
    if (!chan) {
        return TTY_CHAN_ERR_INVALID;
    }

    flags = spin_lock_irqsave(&chan->lock);

    /* count of bytes uncommited */
    len_to_commit = tty_chan_staged(chan);

    /* update the commit position forward to write position */
    chan->commit = chan->write;

    spin_unlock_irqrestore(&chan->lock, flags);

    if (len_to_commit > 0) {
        /* bytes available to read */
        proc_wakeup_all_on(chan->wait_token);
    }

    return len_to_commit;
}

int tty_chan_read(tty_chan_t *chan, uint8_t *out, uint32_t len) {
    irq_flags_t flags;
    uint32_t available_to_read;
    uint32_t len_to_read = len;

    /* check for valid channel */
    if (!chan) {
        return TTY_CHAN_ERR_INVALID;
    }

    /* check for valid buffer to read into */
    if (!out && len > 0) {
        return TTY_CHAN_ERR_INVALID;
    }

    if (len == 0) {
        return 0;
    }

    /* save the context and disable interrupt */
    flags = spin_lock_irqsave(&chan->lock);

    available_to_read = tty_chan_readable(chan);
    if (available_to_read > 0) {
        if (available_to_read < len_to_read) {
            len_to_read = available_to_read;
        }
    } else {
        spin_unlock_irqrestore(&chan->lock, flags);
        return 0;
    }

    /* copy each byte into the out buffer */
    for (uint32_t i = 0; i < len_to_read; i++) {
        out[i] = chan->buf[(chan->read + i) % chan->capacity];
    }

    /* update the reference to read index */
    chan->read += len_to_read;

    /* restore the saved context */
    spin_unlock_irqrestore(&chan->lock, flags);

    return len_to_read;
}
