#pragma once

#include <stdint.h>

static inline uint8_t mmio_read8(const void *base, uint32_t offset) {
    return *(volatile const uint8_t *)((const uint8_t *)base + offset);
}

static inline uint16_t mmio_read16(const void *base, uint32_t offset) {
    return *(volatile const uint16_t *)((const uint8_t *)base + offset);
}

static inline uint32_t mmio_read32(const void *base, uint32_t offset) {
    return *(volatile const uint32_t *)((const uint8_t *)base + offset);
}

static inline void mmio_write8(void *base, uint32_t offset, uint8_t value) {
    *(volatile uint8_t *)((uint8_t *)base + offset) = value;
}

static inline void mmio_write16(void *base, uint32_t offset, uint16_t value) {
    *(volatile uint16_t *)((uint8_t *)base + offset) = value;
}

static inline void mmio_write32(void *base, uint32_t offset, uint32_t value) {
    *(volatile uint32_t *)((uint8_t *)base + offset) = value;
}
