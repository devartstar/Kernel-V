#include "drivers/tty_chan.h"

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
