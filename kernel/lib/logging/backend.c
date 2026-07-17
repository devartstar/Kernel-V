#include "drivers/serial.h"
#include "drivers/vga.h"
#include "lib/logbackend.h"
#include "lib/printk.h"

log_backend_t log_backends[MAX_LOG_BACKENDS];
uint8_t num_logs_backend;

void register_log_backend(log_backend_t backend) {
    if (num_logs_backend < MAX_LOG_BACKENDS) {
        log_backends[num_logs_backend++] = backend;
    }
}

void log_dispatch_to_backends(const char *msg, size_t len, char color) {
    for (uint8_t i = 0; i < num_logs_backend; i++) {
        log_backends[i](msg, len, color);
    }
}

void vga_backend(const char *msg, size_t len, char color) {
    (void)len;
    vga_print_string(msg, color);
}

void ringbuf_backend(const char *msg, size_t len, char color) {
    (void)color;
    ringbuf_write(msg, len);
}

void serial_backend(const char *msg, size_t len, char color) {
    (void)color;
    serial_write(msg, len);
}
