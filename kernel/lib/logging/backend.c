#include "drivers/vga.h"
#include "lib/logbackend.h"
#include "lib/printk.h"

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
    vga_print_string(msg, color);
}

void ringbuf_backend(const char *msg, size_t len, char color) {
    ringbuf_write(msg, len);
}
