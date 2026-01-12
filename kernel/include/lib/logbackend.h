#include <stddef.h>

/* Pointer to a function which takes input - log string and length and returns
 * void */
typedef void (*log_backend_t)(const *msg, size_t len, char color);

#define MAX_LOG_BACKENDS 4

static log_backend_t log_backends[MAX_LOG_BACKENDS];
static uint8_t num_logs_backend;

/*
 * Register logging to a backend
 * @backend - pointer to the method to be called when logging
 *
 * @return void
 */
void register_log_backend(log_backend_t backend);

/*
 * Retarget the log message to all the registered backend sources
 * @char - pointer to a character array of message to log.
 * @len - length of the message to log.
 *
 * @return void
 */
void log_dispatch_to_backends(const char *msg, size_t len, char color);

void vga_backend(const char *msg, size_t len, char color);
void ringbuf_backend(const char *msg, size_t len, char color);
