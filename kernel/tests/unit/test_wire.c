#include "lib/byteorder.h"
#include "lib/printk.h"
#include "mm/kmalloc.h"
#include "net/wire.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t tests_run, tests_passed, tests_failed;

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        tests_run++;                                                           \
        if (cond) {                                                            \
            tests_passed++;                                                    \
            KLOG_INFO("WIRE_TEST", "[TEST %u] [PASS] %s\n", tests_run, msg);   \
        } else {                                                               \
            tests_failed++;                                                    \
            KLOG_ERROR("WIRE_TEST", "[TEST %u] [FAIL] %s\n", tests_run, msg);  \
        }                                                                      \
    } while (0)

/**
 * @bytes_eq is to check first @n bytes from two byte array structure to match
 */
static bool bytes_eq(const uint8_t *a, const uint8_t *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

/**
 * @fill to write the fiest @n bytes to an byte arrat @b with each byte valye @v
 */
static void fill(uint8_t *b, uint32_t n, uint8_t v) {
    for (uint32_t i = 0; i < n; i++) {
        b[i] = v;
    }
}

/* TEST 1: exact byte layout of every st_* and ld_* round trip, at an odd
 * (unaligned) address - the case a pointer cast would get wrong. */
static void test_byteorder(void) {
    uint8_t raw[16];

    /* byte ordering routines alings one byte at a time so bytes starting at
     * mis-aligned address should also be correctly ordered */
    uint8_t *u = raw + 1;

    static const uint8_t be16[] = {0x12, 0x34};
    static const uint8_t le16[] = {0x34, 0x12};
    static const uint8_t be32[] = {0x12, 0x34, 0x56, 0x78};
    static const uint8_t le32[] = {0x78, 0x56, 0x34, 0x12};
    static const uint8_t be64[] = {0x12, 0x34, 0x56, 0x78,
                                   0x9A, 0xBC, 0xDE, 0xF0};
    static const uint8_t le64[] = {0xF0, 0xDE, 0xBC, 0x9A,
                                   0x78, 0x56, 0x34, 0x12};

    st_be16(u, 0x1234);
    TEST_ASSERT(bytes_eq(u, be16, 2) && ld_be16(u) == 0x1234,
                "byteorder: be16 layout + round trip");
    st_le16(u, 0x1234);
    TEST_ASSERT(bytes_eq(u, le16, 2) && ld_le16(u) == 0x1234,
                "byteorder: le16 layout + round trip");
    st_be32(u, 0x12345678u);
    TEST_ASSERT(bytes_eq(u, be32, 4) && ld_be32(u) == 0x12345678u,
                "byteorder: be32 layout + round trip");
    st_le32(u, 0x12345678u);
    TEST_ASSERT(bytes_eq(u, le32, 4) && ld_le32(u) == 0x12345678u,
                "byteorder: le32 layout + round trip");

    st_be64(u, 0x123456789ABCDEF0ull);
    TEST_ASSERT(bytes_eq(u, be64, 8) && ld_be64(u) == 0x123456789ABCDEF0ull,
                "byteorder: be64 layout + round trip (no truncation)");
    st_le64(u, 0x123456789ABCDEF0ull);
    TEST_ASSERT(bytes_eq(u, le64, 8) && ld_le64(u) == 0x123456789ABCDEF0ull,
                "byteorder: le64 layout + round trip (no truncation)");
}

/* TEST 2: sender writes a mixed-endian header, receiver parses it back.
 * Also proves the cursor (not the payload) is untouched by writes. */
static void test_roundtrip(void) {
    uint8_t buf[32] = {0};
    wire_wr_t w;
    wire_rd_t r;

    wire_wr_init(&w, buf, sizeof(buf));
    wire_wr_u8(&w, 0xA5);
    wire_wr_u16be(&w, 0x88B5);
    wire_wr_u16le(&w, 0xBEEF);
    wire_wr_u32le(&w, 0xDEADBEEFu);
    wire_wr_u32be(&w, 0x01020304u);
    wire_wr_u64le(&w, 0x1122334455667788ull);
    uint32_t used = (uint32_t)(w.p - buf);

    TEST_ASSERT(wire_wr_ok(&w) && used == 21 && w.end == buf + sizeof(buf),
                "roundtrip: writer ok, 21 bytes used, end unchanged");
    TEST_ASSERT(buf[1] == 0x88 && buf[2] == 0xB5 && buf[3] == 0xEF &&
                    buf[4] == 0xBE,
                "roundtrip: BE then LE bytes land in the buffer");

    wire_rd_init(&r, buf, used);
    uint8_t a = wire_rd_u8(&r);
    uint16_t b = wire_rd_u16be(&r);
    uint16_t c = wire_rd_u16le(&r);
    uint32_t d = wire_rd_u32le(&r);
    uint32_t e = wire_rd_u32be(&r);
    uint64_t f = wire_rd_u64le(&r);
    TEST_ASSERT(a == 0xA5 && b == 0x88B5 && c == 0xBEEF && d == 0xDEADBEEFu &&
                    e == 0x01020304u && f == 0x1122334455667788ull,
                "roundtrip: reader returns every field intact");
    TEST_ASSERT(wire_rd_done(&r), "roundtrip: reader consumed exactly all");
}

/* TEST 3: short packet. Over-read sets the sticky error, returns 0, and
 * every later read also fails (one check at the end is enough). */
static void test_overrun_sticky(void) {
    uint8_t buf[3] = {1, 2, 3};
    wire_rd_t r;
    wire_rd_init(&r, buf, sizeof(buf));

    uint16_t x = wire_rd_u16le(&r);
    /* only 1 byte left to read */
    uint32_t y = wire_rd_u32le(&r);
    TEST_ASSERT(x == 0x0201 && y == 0 && !wire_rd_ok(&r),
                "overrun: read past end fails and returns 0");
    TEST_ASSERT(r.p == r.end && wire_rd_left(&r) == 0,
                "overrun: cursor parked at end, nothing left");

    /* the byte skipped due to over read must not leak */
    uint8_t z = wire_rd_u8(&r);
    TEST_ASSERT(z == 0 && !wire_rd_ok(&r) && !wire_rd_done(&r),
                "overrun: error is sticky for later reads");
}

/* TEST 4: wrap-around attack. With p+n arithmetic, n = 0xFFFFFFFF wraps the
 * 32-bit address space and looks "in bounds". n > end-p cannot wrap. */
static void test_wrap_attack(void) {
    uint8_t buf[16] = {0};
    wire_rd_t r;

    wire_rd_init(&r, buf, sizeof(buf));
    TEST_ASSERT(wire_rd_bytes(&r, 0xFFFFFFFFu) == NULL && !wire_rd_ok(&r),
                "wrap: take(0xFFFFFFFF) rejected");

    wire_rd_init(&r, buf, sizeof(buf));
    (void)wire_rd_u32le(&r);
    TEST_ASSERT(wire_rd_bytes(&r, 0u - 4u) == NULL && !wire_rd_ok(&r),
                "wrap: take(-4) after advancing rejected");

    wire_rd_init(&r, buf, sizeof(buf));
    TEST_ASSERT(wire_rd_bytes(&r, 0) != NULL && wire_rd_ok(&r) &&
                    wire_rd_left(&r) == 16,
                "wrap: take(0) is legal and moves nothing");
}

/* TEST 5: ok vs done. Trailing bytes are not an error while reading, but a
 * fixed-size message with leftovers is malformed. */
static void test_ok_vs_done(void) {
    uint8_t buf[6] = {0};
    wire_rd_t r;
    wire_rd_init(&r, buf, sizeof(buf));
    (void)wire_rd_u32le(&r);
    TEST_ASSERT(wire_rd_ok(&r) && !wire_rd_done(&r) && wire_rd_left(&r) == 2,
                "ok/done: 2 trailing bytes -> ok but not done");
    (void)wire_rd_u16le(&r);
    TEST_ASSERT(wire_rd_done(&r), "ok/done: consuming the rest -> done");
}

/* TEST 6: sub-cursor confinement. A child over 4 bytes cannot see the 5th,
 * the parent skips the whole 4, and a child failure does not poison it. */
static void test_sub_confinement(void) {
    uint8_t buf[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    wire_rd_t r, sub;
    wire_rd_init(&r, buf, sizeof(buf));

    TEST_ASSERT(wire_rd_sub(&r, 4, &sub) && wire_rd_left(&sub) == 4 &&
                    wire_rd_left(&r) == 4,
                "sub: child sees 4, parent advanced by 4");
    TEST_ASSERT(wire_rd_u32le(&sub) == 0x04030201u && wire_rd_done(&sub),
                "sub: child reads exactly its window");
    TEST_ASSERT(wire_rd_u8(&sub) == 0 && !wire_rd_ok(&sub),
                "sub: child cannot read past its window");
    TEST_ASSERT(wire_rd_ok(&r) && wire_rd_u8(&r) == 5,
                "sub: parent unaffected by child error");
}

/* TEST 7: lying length. Header claims more bytes than the packet holds:
 * both parent and child must end up failed. */
static void test_sub_lying_len(void) {
    uint8_t buf[4] = {0};
    wire_rd_t r, sub;
    wire_rd_init(&r, buf, sizeof(buf));

    TEST_ASSERT(!wire_rd_sub(&r, 100, &sub) && !wire_rd_ok(&r) &&
                    !wire_rd_ok(&sub) && wire_rd_left(&sub) == 0,
                "sub: lying length fails parent and child");
}

/* TEST 8: writer overflow. All-or-nothing: no partial field is written, the
 * error is sticky, and the bytes beyond capacity are never touched. */
static void test_writer_overflow(void) {
    uint8_t buf[8];
    wire_wr_t w;
    fill(buf, sizeof(buf), 0xCC);
    wire_wr_init(&w, buf, 4); /* only the first 4 bytes are ours */

    wire_wr_u64be(&w, 0x1122334455667788ull);
    TEST_ASSERT(!wire_wr_ok(&w) && w.p == w.end,
                "wr overflow: 8 into 4 fails, cursor parked");
    TEST_ASSERT(buf[0] == 0xCC && buf[3] == 0xCC && buf[4] == 0xCC,
                "wr overflow: no partial write, nothing past cap");

    wire_wr_u8(&w, 0x01);
    TEST_ASSERT(!wire_wr_ok(&w) && buf[0] == 0xCC,
                "wr overflow: error sticky for later writes");
}

/* TEST 9: reserve + back-patch. The pattern for length fields whose value is
 * only known after the body is written.
 * Back-patching steps:
 * 1. Reserve space for length. (length represent number of bytes for a variable
 * body.
 * 2. Write the variable body (multiple writes maybe)
 * 3. Backtrack and fill the reserved length section with size of body.
 * Example:
 *      Offset:    0   1 │  2   3   4   5 │  6
 *      Bytes:    05  00 │ BE  BA  FE  CA │ 7F
 *                └─┬──┘  └──────┬──────┘
 *               length      0xCAFEBABE
 */
static void test_reserve_backpatch(void) {
    uint8_t buf[16] = {0};
    wire_wr_t w;
    wire_rd_t r;
    wire_wr_init(&w, buf, sizeof(buf));

    uint8_t *len_slot = wire_wr_reserve(&w, 2);
    uint8_t *body = w.p;
    wire_wr_u32le(&w, 0xCAFEBABEu);
    wire_wr_u8(&w, 0x7F);
    uint16_t body_len = (uint16_t)(w.p - body);
    TEST_ASSERT(len_slot != NULL && wire_wr_ok(&w) && body_len == 5,
                "reserve: slot reserved, 5 body bytes written");
    st_le16(len_slot, body_len);

    wire_rd_t body_r;
    wire_rd_init(&r, buf, (uint32_t)(w.p - buf));
    uint16_t n = wire_rd_u16le(&r);
    TEST_ASSERT(n == 5 && wire_rd_sub(&r, n, &body_r) &&
                    wire_rd_u32le(&body_r) == 0xCAFEBABEu &&
                    wire_rd_u8(&body_r) == 0x7F && wire_rd_done(&body_r) &&
                    wire_rd_done(&r),
                "reserve: back-patched length parses back");
}

/* TEST 10: TLV options - the protocol-evolution mechanism.
 * A "receiver" that only knows type 0x01 must: use 0x01, skip unknown
 * non-critical 0x02, reject unknown critical 0x83. Then: truncated TLV. */
#define T_KNOWN 0x01
#define T_OPTIONAL 0x02
#define T_MUST_KNOW (0x03 | WIRE_TLV_CRITICAL)

static void test_tlv(void) {
    uint8_t buf[32] = {0};
    wire_wr_t w;
    wire_rd_t r, val;
    uint8_t type = 0;
    const uint8_t opt[3] = {9, 9, 9};
    uint32_t known_val = 0x11223344u;

    wire_wr_init(&w, buf, sizeof(buf));
    uint8_t kv[4];
    st_le32(kv, known_val);
    wire_wr_tlv(&w, T_KNOWN, kv, 4);
    wire_wr_tlv(&w, T_OPTIONAL, opt, 3);
    uint32_t used = (uint32_t)(w.p - buf);
    TEST_ASSERT(wire_wr_ok(&w) && used == (3 + 4) + (3 + 3) &&
                    buf[0] == T_KNOWN && buf[1] == 4 && buf[2] == 0,
                "tlv: [type u8][len u16 LE][value] layout");

    /* Receiver loop: known + skippable optional -> accepted. */
    wire_rd_init(&r, buf, used);
    bool seen_known = false, rejected = false;
    uint32_t got = 0;
    while (wire_rd_tlv(&r, &type, &val)) {
        if (type == T_KNOWN) {
            got = wire_rd_u32le(&val);
            seen_known = wire_rd_done(&val);
        } else if (type & WIRE_TLV_CRITICAL) {
            rejected = true;
        } /* else: unknown optional -> ignore val, loop already skipped it */
    }
    TEST_ASSERT(wire_rd_ok(&r) && wire_rd_done(&r) && seen_known &&
                    got == known_val && !rejected,
                "tlv: known parsed, unknown optional skipped");

    /* Append an unknown critical option -> receiver must reject. */
    wire_wr_tlv(&w, T_MUST_KNOW, NULL, 0);
    used = (uint32_t)(w.p - buf);
    wire_rd_init(&r, buf, used);
    rejected = false;
    while (wire_rd_tlv(&r, &type, &val)) {
        if (type != T_KNOWN && (type & WIRE_TLV_CRITICAL)) {
            rejected = true;
        }
    }
    TEST_ASSERT(wire_rd_ok(&r) && rejected,
                "tlv: unknown critical option detected");

    /* Truncated: header claims 10 value bytes, only 2 follow. */
    uint8_t bad[5] = {T_KNOWN, 10, 0, 0xAA, 0xBB};
    wire_rd_init(&r, bad, sizeof(bad));
    bool got_one = wire_rd_tlv(&r, &type, &val);
    TEST_ASSERT(!got_one && !wire_rd_ok(&r) && !wire_rd_ok(&val),
                "tlv: truncated value -> error, not a clean end");

    /* Truncated header: only type byte present. */
    uint8_t bad2[1] = {T_KNOWN};
    wire_rd_init(&r, bad2, sizeof(bad2));
    TEST_ASSERT(!wire_rd_tlv(&r, &type, &val) && !wire_rd_ok(&r),
                "tlv: truncated header -> error");
}

void run_wire_tests(void) {
    tests_run = tests_passed = tests_failed = 0;
    KLOG_VERBOSE("WIRE_TEST", "Starting wire tests....\n");

    test_byteorder();
    test_roundtrip();
    test_overrun_sticky();
    test_wrap_attack();
    test_ok_vs_done();
    test_sub_confinement();
    test_sub_lying_len();
    test_writer_overflow();
    test_reserve_backpatch();
    test_tlv();

    KLOG_INFO("WIRE_TEST", "wire tests: run=%u passed=%u failed=%u\n",
              tests_run, tests_passed, tests_failed);
    KLOG_VERBOSE("WIRE_TEST", "Completed wire tests....\n");
}
