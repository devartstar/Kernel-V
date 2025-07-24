#include "kernel/printk.h"
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#define VAG_BUFFER  0xB8000
#define VGA_HEIGHT  25
#define VGA_WIDTH   80
#define VGA_SIZE    (VGA_HEIGHT * VGA_WIDTH * 2)

// circular log buffer 
static char log_buffer[LOG_BUF_SIZE];

// position of next byte that will be written
static size_t rb_head = 0;
// position of the oldest byte in the buffer
static size_t rb_tail = 0;

// append one character into the circular ring buffer
static void ringbuf_putc(char ch)
{
	log_buffer[rb_head] = ch;
	rb_head = (rb_head + 1) % LOG_BUF_SIZE;

	// Buffer has become full advance the tail to drop oldest byte
	if(rb_head == rb_tail)
	{
		rb_tail = (rb_tail + 1) % LOG_BUF_SIZE;
	}
}

// dump a string into the ring buffer
static void ringbuf_write(cosnt char* str, size_t str_len)
{
	for(size_t i = 0; i < str_lem; i++)
	{
		ringbuf_putc(str[i]);
	}
}

// Basic console write to VGA Text Memory




