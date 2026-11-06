#ifndef WIRE_H
#define WIRE_H

#include "net/wire_read.h"
#include "net/wire_write.h"

/* ----- TLV options: then extensibility stream ------ */

/**
 * LAYOUT: [type u8] [len u16 order LE] [value: len bytes]
 * type & WIRE_TLV_CRITICAL: a reciever that does not understand this option
 * MUST reject the message; otherwise it MAY skip it.
 * This lets newer nodes add optional feature without breaking older nodes, and
 * add mandatory ones without being silenetly misread
 */
#define WIRE_TLV_CRITICAL 0x80u

/**
 * @wire_rd_tlv reads from wire at cursor @r till the end and update the read
 * cursor.
 *
 * @r the read cursor in wire
 * @type read from the wire
 * @val read from the wire
 *
 * @returns false at clean end of input OR an error
 * check wire_rd_ok(r) after the loop to tell them apart
 */
static inline bool wire_rd_tlv(wire_rd_t *r, uint8_t *type, wire_rd_t *val) {
    if (r->err || wire_rd_left(r) == 0) {
        return false;
    }

    /* get the address ref to the type byte */
    *type = wire_rd_u8(r);

    /* get the len of bytes to be read from wire */
    uint16_t len = wire_rd_u16le(r);

    /* copy len bytes from wire at cursor @r to another wire buffer with cursor
     * @val and update cursor of @r */
    return wire_rd_sub(r, len, val);
}

/**
 * @wire_wr_tlv write value @val to write cusroe at @w
 * write the buffer in tlv format.
 *
 * @w write cursor in the wire
 * @type type to be added to the wire
 * @val data to be added to the wire
 * @len length of data to be added to wire
 */
static inline void wire_wr_tlv(wire_wr_t *w, uint8_t type, const void *val,
                               uint16_t len) {
    wire_wr_u8(w, type);
    wire_wr_u8(w, len);
    wire_wr_bytes(w, val, len);
}

#endif /* WIRE_H */
