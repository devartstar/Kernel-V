#ifndef WIRE_WRITE_H
#define WIRE_WRITE_H

#include "lib/byteorder.h"
#include "lib/string.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------ Writer (our own output) ------ */

/**
 * @wire_wr - cursor for writing bytes to another machine send through wire.
 * Note! it is not a memory in storage or memory. (ie. doesnt hold bytes)
 *
 * @p is the next byte to write
 * @end is the one past the last byte
 * @err sticky set on first out of bound access
 */
typedef struct wire_wr {
    uint8_t *p;
    uint8_t *end;
    bool err;
} wire_wr_t;

/**
 * @wire_wr_initialize initializes the structure for wire outgoing bytes write
 *
 * @r wire write cursor structure
 * @buf buffer to be sent through the wire
 * @cap max length of buffer
 */
static inline void wire_wr_init(wire_wr_t *w, void *buf, uint32_t cap) {
    w->p = (uint8_t *)buf;
    w->end = w->p + cap;
    w->err = (buf == NULL && cap != 0);
}

/**
 * @wire_wr_ok return 1 when the outgoing buffer via wire is ok else 0
 */
static inline bool wire_wr_ok(const wire_wr_t *w) { return !w->err; }

/**
 * @wire_wr_reserve reserver @n bytes for writing to the wire and return the
 * starting address to start writing.
 *
 * @w is ref to write cursor
 * @n number of bytes to write
 *
 * @return ref to the start writing position
 */
static inline uint8_t *wire_wr_reserve(wire_wr_t *w, uint32_t n) {
    if (w->err || n > (uint32_t)(w->end - w->p)) {
        w->err = true;
        w->p = w->end;
        return NULL;
    }

    uint8_t *q = w->p;
    w->p += n;
    return q;
}

/**
 * @wire_wr_u8 writes 1 byte to the wire.
 * byte ordering does't matter for one byte.
 */
static inline void wire_wr_u8(wire_wr_t *w, uint8_t v) {
    /* get the memory referenced by read and update read position by 1 byte */
    uint8_t *q = wire_wr_reserve(w, 1);

    if (q) {
        q[0] = v;
    }
}

static inline void wire_wr_u16le(wire_wr_t *w, uint16_t v) {
    uint8_t *q = wire_wr_reserve(w, 2);
    if (q) {
        st_le16(q, v);
    }
}

static inline void wire_wr_u32le(wire_wr_t *w, uint32_t v) {
    uint8_t *q = wire_wr_reserve(w, 4);
    if (q) {
        st_le32(q, v);
    }
}

static inline void wire_wr_u64le(wire_wr_t *w, uint64_t v) {
    uint8_t *q = wire_wr_reserve(w, 8);
    if (q) {
        st_le64(q, v);
    }
}

static inline void wire_wr_u16be(wire_wr_t *w, uint16_t v) {
    uint8_t *q = wire_wr_reserve(w, 2);
    if (q) {
        st_be16(q, v);
    }
}

static inline void wire_wr_u32be(wire_wr_t *w, uint32_t v) {
    uint8_t *q = wire_wr_reserve(w, 4);
    if (q) {
        st_be32(q, v);
    }
}

static inline void wire_wr_u64be(wire_wr_t *w, uint64_t v) {
    uint8_t *q = wire_wr_reserve(w, 8);
    if (q) {
        st_be64(q, v);
    }
}

/**
 * @wire_wr_bytes reserves n bytes in the parent cursor and move data forward by
 * n bytes and in copy the parent n bytes from buffer passed
 */
static inline void wire_wr_bytes(wire_wr_t *w, const void *src, uint32_t n) {
    uint8_t *q = wire_wr_reserve(w, n);
    if (q && n) {
        memcpy(q, src, n);
    }
}

#endif /* WIRE_WRITE_H */
