#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "arch/x86/mmio.h"
#include "lib/printk.h"
#include "tests/test_mmio.h"

int test_mmio_helpers_basic_32b(void) {
    uint8_t fake_mmio[32];
    memset(fake_mmio, 0, sizeof(fake_mmio));

    void *base = fake_mmio;

    /* 32 bits write/read at offset 0x0 */
    mmio_write32(base, 0x0, 0x11223344);
    uint32_t v32 = mmio_read32(base, 0x0);
    if (v32 != 0x11223344) {
        KLOG_ERROR("TEST",
                   "mmio_write32/read32 failed: got incorrect value: 0x%08x "
                   "(expected value: 0x11223344)\n",
                   v32);
        return -1;
    }

    /* Check raw byte layout (little-endian on x86) */
    if (fake_mmio[0x0] != 0x44 || fake_mmio[0x1] != 0x33 ||
        fake_mmio[0x2] != 0x22 || fake_mmio[0x3] != 0x11) {
        KLOG_ERROR("TEST", "mmio_write32 raw byte layout wrong\n");
        return -1;
    }

    KLOG_INFO("TEST", "mmio helper basic test for 32 bits passed\n");
    return 0;
}

int test_mmio_helpers_basic_16b(void) {
    uint8_t fake_mmio[32];
    memset(fake_mmio, 0, sizeof(fake_mmio));

    void *base = fake_mmio;

    /* 16 bits write/read at offset 0x0 */
    mmio_write16(base, 0x6, 0xA1b2);
    uint32_t v16 = mmio_read16(base, 0x6);
    if (v16 != 0xA1B2) {
        KLOG_ERROR("TEST",
                   "mmio_write16/read16 failed: got incorrect value: 0x%08x "
                   "(expected value: 0xA1B2)\n",
                   v16);
        return -1;
    }

    /* Check raw byte layout (little-endian on x86) */
    if (fake_mmio[0x6] != 0xB2 || fake_mmio[0x7] != 0xA1) {
        KLOG_ERROR("TEST", "mmio_write16 raw byte layout wrong\n");
        return -1;
    }

    /* Checking neighbouring byte */
    if (fake_mmio[0x8] != 0x00) {
        KLOG_ERROR("TEST",
                   "mmio_write16 at 0x06 corrupted neighbor byte at 0x8\n");
        return -1;
    }

    KLOG_INFO("TEST", "mmio helper basic test for 16 bits passed\n");
    return 0;
}

int test_mmio_helpers_basic_8b(void) {
    uint8_t fake_mmio[32];
    memset(fake_mmio, 0, sizeof(fake_mmio));

    void *base = fake_mmio;

    /* 8 bits write/read at offset 0x0 */
    mmio_write8(base, 0x4, 0x5A);
    uint32_t v8 = mmio_read8(base, 0x4);
    if (v8 != 0x5A) {
        KLOG_ERROR("TEST",
                   "mmio_write8/read8 failed: got incorrect value: 0x%08x "
                   "(expected value: 0x5A)\n",
                   v8);
        return -1;
    }

    /* Check raw byte layout (little-endian on x86) */
    if (fake_mmio[0x5] != 0x00) {
        KLOG_ERROR("TEST",
                   "mmio_write8 at 0x04 corrupted neighbouring byte at 0x05\n");
        return -1;
    }

    KLOG_INFO("TEST", "mmio helper basic test for 8 bits passed\n");
    return 0;
}
