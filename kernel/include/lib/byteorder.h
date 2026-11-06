#ifndef BYTEORDER_H
#define BYTEORDER_H

#include <stdint.h>

/**
 * 32 bit number 0x11223344 takes 4 bytes in memory
 * There are two ways to order (Address) +0  +1  +2  +3
 * Network Byte Order (Big Endian)       11  22  33  44
 * Host Byte Order (Little Endian)       44  33  22  11
 *
 * 1. Explicit Endian - function name stats the ordering.
 *
 * 2. Alignment - consider packed structs not starting at a nice address
 * boundary(ie. multiple of 4), it can cause problems with unaligned reads:
 *   2.1 Software: reading field across addr boundary - undefined behaviour
 *   2.2 Hardware: x86 tolerates but ARM, RISC-V faults.
 *
 * 3. Aliasing - C has strict aliasing, not allowing to read memory thru a
 * pointer of unrelated type.
 *
 * 4. GCC -O2 folds instructions of byte by byte ordering operations into a
 * single mov + bswap.
 *
 * 5. Ethernet and IP headers are Big Endian while Payload is in Little Endian.
 *
 * founction name:
 * <direction>_<order><size>:
 * direction:
 *  - ld is for memory to CPU
 *  - st for CPU to memory
 */

/* ------ Load address in Big Endian ----- */

static inline uint16_t ld_be16(const void *p) {
    /* array of bytes */
    const uint8_t *b = p;
    return (uint16_t)((b[0] << 8) | b[1]);
}

static inline uint32_t ld_be32(const void *p) {
    const uint8_t *b = p;
    return (((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
            ((uint32_t)b[2] << 8) | ((uint32_t)b[3]));
}

static inline uint32_t ld_be64(const void *p) {
    const uint8_t *b = p;
    return (((uint64_t)ld_be32(b) << 32) | (uint64_t)ld_be32(b + 4));
}

/* ------ Load address in Little Endian ----- */

static inline uint16_t ld_le16(const void *p) {
    /* array of bytes */
    const uint8_t *b = p;
    return (uint16_t)((b[1] << 8) | b[0]);
}

static inline uint32_t ld_le32(const void *p) {
    const uint8_t *b = p;
    return (((uint32_t)b[3] << 24) | ((uint32_t)b[2] << 16) |
            ((uint32_t)b[1] << 8) | ((uint32_t)b[0]));
}

static inline uint32_t ld_le64(const void *p) {
    const uint8_t *b = p;
    return ((uint64_t)ld_le32(b) | ((uint64_t)ld_le32(b + 4) << 32));
}

/* ------ Store Value in Big Endian ------- */

static inline void st_be16(void *p, uint16_t v) {
    uint8_t *b = p;
    b[0] = (uint8_t)(v >> 8);
    b[1] = (uint8_t)v;
}

static inline void st_be32(void *p, uint32_t v) {
    uint8_t *b = p;
    b[0] = (uint8_t)(v >> 24);
    b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >> 8);
    b[3] = (uint8_t)v;
}

static inline void st_be64(void *p, uint64_t v) {
    st_be32(p, (uint32_t)(v >> 32));
    st_be32((uint8_t *)p + 4, (uint32_t)v);
}

/* ------- Store Value in Little Endian ------- */

static inline void st_le16(void *p, uint16_t v) {
    uint8_t *b = p;
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
}

static inline void st_le32(void *p, uint32_t v) {
    uint8_t *b = p;
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16);
    b[3] = (uint8_t)(v >> 24);
}

static inline void st_le64(void *p, uint64_t v) {
    st_le32(p, (uint32_t)v);
    st_le32((uint8_t *)p + 4, (uint32_t)(v >> 32));
}

#endif /* BYTEORDER_H */
