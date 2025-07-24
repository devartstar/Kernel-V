#ifndef DRIVERS_VGA_H
#define DRIVERS_VGA_H

#include <stdint.h>

/**
 * VGA is a hardware standard and graphic controller.
 * Hardware standard : common specification how video hardware should behave.
 *      VGA's video RAM (VRAM) is in graphics adapter and not in the DRAM. 
 *      In Text Mode: CPU writes characters and colors into VRAM at address 0xb8000
 *      In Graphics Mode: CPU writes pixel data into VRAM at address 0xa0000
 * Associated graphic controller : physical chip that implements the rules.
 *      Read the screen buffer in VRAM.
 *      Convert to video signals
 *      Send signals to the screen.
 */

// VGA Text Mode Constants
#define VGA_ADDRESS     0xb8000
#define VGA_WIDTH       80
#define VGA_HEIGHT      25
// 2 bytes per character (char + color attribute)
#define VGA_SIZE        (VGA_WIDTH * VGA_HEIGHT * 2)

// VGA I/O Ports
#define VGA_CTRL_PORT   0x3d4
#define VGA_DATA_PORT   0x3d5

// VGA Color Definitions
enum vga_color {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW        = 14,
    VGA_WHITE         = 15,
};

// VGA Color Macros
#define VGA_COLOR(bg, fg)       (((bg) << 0x04) | ((fg) & 0x0f))
#define WHITE_ON_BLACK          VGA_COLOR(VGA_BLACK, VGA_WHITE)
#define RED_ON_WHITE            VGA_COLOR(VGA_WHITE, VGA_RED)
#define BLUE_ON_YELLOW          VGA_COLOR(VGA_YELLOW, VGA_BLUE)
#define GREEN_ON_BLACK          VGA_COLOR(VGA_BLACK, VGA_GREEN)
#define YELLOW_ON_BLACK         VGA_COLOR(VGA_BLACK, VGA_YELLOW)

// VGA Driver Functions
void vga_init(void);
void vga_clear_screen(void);
void vga_put_char(char c, char color);
void vga_print_string(const char* str, char color);
void vga_move_cursor(void);
void vga_scroll_up(void);
void vga_set_cursor_position(int row, int col);
void vga_get_cursor_position(int* row, int* col);

#endif /* DRIVERS_VGA_H */
