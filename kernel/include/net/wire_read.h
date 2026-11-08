#ifndef WIRE_READ_H
#define WIRE_READ_H

#include "lib/byteorder.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------ Reader (untrusted input) ------ */

/**
 * @wire_rd - cursor for reading bytes from another machine arrived through
 * wire. these bytes cannot be trusted so needs to read via cursor and verify.
 * Note! it is not a memory in storage or memory. (ie. doesnt hold bytes)
 *
 * @p stores the physical adress of the next byte to be read in wire.
 * the byte itself cannot be modified through p, hence const
 * @end is the address of one past the last byte, byte is un-modifiable hence
 * const
 * @err sticky set on first out of bound access
 */
typedef struct wire_rd {
    const uint8_t *p;
    const uint8_t *end;

    bool err;
} wire_rd_t;

/**
 * @wire_rd_initialize initializes the structure for wire incomming bytes read
 *
 * @r wire read cursor structure
 * @buf buffer arrived through the wire
 * @len length of buffer arrived and will be read
 */
static inline void wire_rd_init(wire_rd_t *r, const void *buf, uint32_t len) {
    r->p = (uint8_t *)buf;
    r->end = r->p + len;
    r->err = (buf == NULL && len != 0);
}

/**
 * @wire_rd_left return the number of bytes arrived via wire nd is yet to be
 * read */
static inline uint32_t wire_rd_left(const wire_rd_t *r) {
    return (r->err == 0 ? (uint32_t)(r->end - r->p) : 0);
}

/**
 * @wire_rd_ok return 1 when the incimming buffer via wire is ok else 0
 */
static inline bool wire_rd_ok(const wire_rd_t *r) { return !r->err; }

/**
 * @wire_rd_done completed reading all bytes arrived via wire and no error will
 * return 1 else 0
 */
static inline bool wire_rd_done(const wire_rd_t *r) {
    return (r->p == r->end) && !r->err;
}

/**
 * @wire_rd_take is the ONLY bound check. returns the read position and update
 * by @n.
 * Compare @n against the remaining count. Neded because if n is modified by
 * attacher, @r->p + @n can wrap past 4GB and look in-bounds
 *
 * @returns the position to start reading.
 */
static inline const uint8_t *wire_rd_take(wire_rd_t *r, uint32_t n) {
    if (r->err || n > (uint32_t)(r->end - r->p)) {
        r->err = true;
        r->p = r->end;
        return NULL;
    }

    const uint8_t *q = r->p;
    r->p += n;
    return q;
}

/**
 * @wire_rd_u8 reads 1 byte from the wire and returns the valur
 * byte ordering does't matter for one byte.
 */
static inline uint8_t wire_rd_u8(wire_rd_t *r) {
    /* get the memory referenced by read and update read position by 1 byte */
    const uint8_t *q = wire_rd_take(r, 1);

    return q ? q[0] : 0;
}

static inline uint16_t wire_rd_u16le(wire_rd_t *r) {
    const uint8_t *q = wire_rd_take(r, 2);
    return q ? ld_le16(q) : 0;
}

static inline uint32_t wire_rd_u32le(wire_rd_t *r) {
    const uint8_t *q = wire_rd_take(r, 4);
    return q ? ld_le32(q) : 0;
}

static inline uint64_t wire_rd_u64le(wire_rd_t *r) {
    const uint8_t *q = wire_rd_take(r, 8);
    return q ? ld_le64(q) : 0;
}

static inline uint16_t wire_rd_u16be(wire_rd_t *r) {
    const uint8_t *q = wire_rd_take(r, 2);
    return q ? ld_be16(q) : 0;
}

static inline uint32_t wire_rd_u32be(wire_rd_t *r) {
    const uint8_t *q = wire_rd_take(r, 4);
    return q ? ld_be32(q) : 0;
}

static inline uint64_t wire_rd_u64be(wire_rd_t *r) {
    const uint8_t *q = wire_rd_take(r, 8);
    return q ? ld_be64(q) : 0;
}

/**
 * @wire_rd_bytes returns the address for the cursor to read and update ref by
 * @len bytes
 */
static inline const uint8_t *wire_rd_bytes(wire_rd_t *r, uint32_t len) {
    return wire_rd_take(r, len);
}

/**
 * @wire_rd_sub creates a nested cursor over the next @n bytes and advances the
 * parent cursor by @n bytes.
 * Child cursor can only read upto @n bytes starting from @r->data even if @r
 * has more data.
 *
 * @r is cursor to parent wire read
 * @n is number of bytes to read from wire
 * @sub is nester cursor initialized with only @n bytes starting from @r->data
 *
 * @returns true when allocation to @sub is success.
 */
static inline bool wire_rd_sub(wire_rd_t *r, uint32_t n, wire_rd_t *sub) {
    const uint8_t *q = wire_rd_take(r, n);
    wire_rd_init(sub, q, q ? n : 0);
    sub->err = (q == NULL);
    return q != NULL;
}

#endif /* WIRE_READ_H */
