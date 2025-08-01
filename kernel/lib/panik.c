#include <stdarg.h>
#include "../include/printk.h"
#include "../include/drivers/vga.h"

// Panic mode and state tracking
static panik_mode_t current_panik_mode = PANIC_MODE_NORMAL;
static panik_state_t panik_state = {0};

/**
 * Set panic mode for testing
 */
void set_panik_mode(panik_mode_t mode)
{
    current_panik_mode = mode;
}

/**
 * Get current panic mode
 */
panic_mode_t get_panik_mode(void)
{
    return current_panik_mode;
}

/**
 * Get panic state for testing
 */
const panic_state_t* get_panik_state(void)
{
    return &panik_state;
}

/**
 * Reset panic state (for testing)
 */
void reset_panik_state(void)
{
    panik_state.panik_called = 0;
    panik_state.panik_call_count = 0;
    panik_state.last_panik_msg[0] = '\0';
}

__attribute__((noreturn))
void panik(const char* fmt, ...)
{
	// update panik state
	panik_state.panik_called = 1;
	panik_state.panik_call_count++;

	// format the panik message
	va_list args;
	va_start(args, fmt);
	int len = my_vsnprintf(panik_state.last_panik_msg, sizeof(panik_state.last_panik_msg), fmt, args);
	va_end(args);

	// In Test Mode
	if (current_panik_mode == PANIK_MODE_TEST)
	{
		pr_emerg("[TEST PANIK] %s\n", panik_state.last_panik_msg);
		return;
	}

	// Disable Interrupt in PANIK_MODE_NORMAL
	__asm__ __volatile__("cli");

	// Log to ring buffer and display
	ringbuf_write("[PANIK] ", 8);
	ringbuf_write(panik_state.last_panik_msg, len);
	ringbuf_write("\n", 1);


	vga_print_string("[PANIC] ", VGA_COLOR(VGA_RED, VGA_WHITE));
	vga_print_string(panic_state.last_panic_msg, VGA_COLOR(VGA_BLACK, VGA_LIGHT_RED));
	vga_print_string("\n", WHITE_ON_BLACK);

	// Print system halt message
	vga_print_string("System halted. Press reset to restart.\n", VGA_COLOR(VGA_BLACK, VGA_YELLOW));

	// halt in panik
	while (1) {
		__asm__ __volatile__("hlt");
	}
}
