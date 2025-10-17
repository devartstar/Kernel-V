#include "drivers/vga.h"

// Global cursor position
static int cursor_row = 0;
static int cursor_col = 0;

// Inline function to write to I/O ports
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * Initialize VGA driver
 */
void vga_init(void) {
    cursor_row = 0;
    cursor_col = 0;
    vga_clear_screen();
    vga_move_cursor();
}

/**
 * Clear the entire screen
 */
void vga_clear_screen(void) {
    volatile unsigned char* vga = (unsigned char*)VGA_ADDRESS;
    
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i * 2] = ' ';           // Character
        vga[i * 2 + 1] = WHITE_ON_BLACK;  // Attribute
    }
    
    cursor_row = 0;
    cursor_col = 0;
    vga_move_cursor();
}

/**
 * Set cursor position
 */
void vga_set_cursor_position(int row, int col) {
    if (row >= 0 && row < VGA_HEIGHT && col >= 0 && col < VGA_WIDTH) {
        cursor_row = row;
        cursor_col = col;
        vga_move_cursor();
    }
}

/**
 * Get current cursor position
 */
void vga_get_cursor_position(int* row, int* col) {
    if (row) *row = cursor_row;
    if (col) *col = cursor_col;
}

/**
 * Move hardware cursor to current position
 * In VGA the cursor position is a 16 bit value. Each CRTC register is 8 bits.
 */
void vga_move_cursor(void) {
    int current_pos = cursor_row * VGA_WIDTH + cursor_col;

    // Write cursor position low 8 bits
    outb(VGA_CTRL_PORT, 0x0f);
    outb(VGA_DATA_PORT, (uint8_t)(current_pos & 0xff));

    // Write cursor position high 8 bits
    outb(VGA_CTRL_PORT, 0x0e);
    outb(VGA_DATA_PORT, (uint8_t)((current_pos >> 8) & 0xff));
}

/**
 * Display a character to the screen using VGA
 * Each character needs 2 bytes - character & color attribute
 */
void vga_put_char(char c, char color) {
    volatile unsigned char* vga = (unsigned char*)VGA_ADDRESS;
    int offset = 2 * (cursor_row * VGA_WIDTH + cursor_col);
    vga[offset] = c;
    vga[offset + 1] = color;
}

/**
 * Scroll screen up by one line
 * Move all rows up by 1 and clear the last line
 */
void vga_scroll_up(void) {
    volatile unsigned char* vga = (unsigned char*)VGA_ADDRESS;
    
    // Move all rows one line up
    for (int row = 1; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            int src_offset = 2 * (row * VGA_WIDTH + col);
            int dst_offset = 2 * ((row - 1) * VGA_WIDTH + col);
            vga[dst_offset] = vga[src_offset];           // Character
            vga[dst_offset + 1] = vga[src_offset + 1];   // Attribute
        }
    }
    
    // Clear the last line
    for (int col = 0; col < VGA_WIDTH; col++) {
        int offset = 2 * ((VGA_HEIGHT - 1) * VGA_WIDTH + col);
        vga[offset] = ' ';
        vga[offset + 1] = WHITE_ON_BLACK;
    }
}

/**
 * Print a string to the screen with automatic line wrapping and scrolling
 */
void vga_print_string(const char* str, char color) {
    while (*str) {
        if (*str == '\n') {
            cursor_row++;
            cursor_col = 0;
        } else {
            vga_put_char(*str, color);
            cursor_col++;
            
            // Handle line wrapping
            if (cursor_col >= VGA_WIDTH) {
                cursor_row++;
                cursor_col = 0;
            }
        }
        
        // Handle scrolling
        if (cursor_row >= VGA_HEIGHT) {
            vga_scroll_up();
            cursor_row = VGA_HEIGHT - 1;
            cursor_col = 0;
        }
        
        vga_move_cursor();
        str++;
    }
}
