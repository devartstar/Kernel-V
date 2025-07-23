#include <stdint.h>

/**
 * VGA is a hardware standard and graphic controller.
 * Hardware standard : common specification how video hardware should behave.
 *      VGA's video RAM (VRAM) is in graphics adapter and not in the DRAM. 
 *      In Text Mode: CPU writes characters and colors into VRAM at address 0xb8000
 *      In Graphics Mode: CPU writes pixel data into VRAM at address 0xa0000
 * Associated graphic controller : physical chip than implements the rules.
 *      Read the screen buffer in VRAM.
 *      Convert to video signals
 *      Send signals to the screen.
 */
#define VGA_ADDRESS     0xb8000
#define VGA_WIDTH       80
#define VGA_HEIGHT      25

enum vga_color {
  VGA_BLACK       = 0,
  VGA_BLUE        = 1,
  VGA_GREEN       = 2,
  VGA_CYAN        = 3,
  VGA_RED         = 4,
  VGA_MAGENTA     = 5,
  VGA_BROWN       = 6,
  VGA_LIGHT_GREY  = 7,
  VGA_DARK_GREY   = 8,
  VGA_LIGHT_BLUE  = 9,
  VGA_LIGHT_GREEN = 10,
  VGA_LIGHT_CYAN  = 11,
  VGA_LIGHT_RED   = 12,
  VGA_LIGHT_MAGENTA = 13,
  VGA_YELLOW      = 14,
  VGA_WHITE       = 15,
};

#define VGA_COLOR(bg, fg)       (((bg) << 0x04) | ((fg) & 0x0f))
#define WHITE_ON_BLACK          VGA_COLOR(VGA_BLACK, VGA_WHITE)
#define RED_ON_WHITE            VGA_COLOR(VGA_WHITE, VGA_RED)
#define BLUE_ON_YELLOW          VGA_COLOR(VGA_YELLOW, VGA_BLUE)

/**
 * I/O Ports for VGA
 * Control Port: write the register number here to access 1 reg from CRTC
 * register block (~25 regs).
 * Data Port: to read / to write value to that selected register in ctrl port
 */
#define VGA_CTRL_PORT 0x3d4
#define VGA_DATA_PORT 0x3d5

static int cursor_row = 0;
static int cursor_col = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * In VGA the cursor position is a 16 bit value. Each CRTC register is 8 bits.
 */
void move_cursor()
{
    int current_pos = cursor_row * VGA_WIDTH + cursor_col;

    // to write to cursors low 8 bits
    outb(VGA_CTRL_PORT, 0x0f);
    outb(VGA_DATA_PORT, (uint8_t) (current_pos & 0xff));

    // write cursor positionn high bits
    outb(VGA_CTRL_PORT, 0x0e);
    outb(VGA_DATA_PORT, (uint8_t) ((current_pos >> 8) & 0xff));
}

/**
* Displays a character to the screen using VGA (Video graphis array)
* Displaying Each character needs 2 characters - character & color
* c char - character to display on screen
* color char - 4 bits field for color
*/
static inline void put_char(char c, char color)
{
    volatile unsigned char* vga = (unsigned char*)VGA_ADDRESS;
    int offset = 2 * (cursor_row * VGA_WIDTH + cursor_col);
    vga[offset] = c;
    vga[offset+1] = color;
}

/**
* To scroll up move up all the rows by 1
* clear up the last line
*/
void scroll_up()
{
    volatile unsigned char* vga = (unsigned char*)VGA_ADDRESS;
    int disp_row;
    int disp_col;

    //
    // Move all the rows one line up
    //
    for(disp_row = 1; disp_row < VGA_HEIGHT; disp_row++)
    {
        for(disp_col = 0; disp_col < VGA_WIDTH; disp_col++)
        {
            int disp_from = 2 * (disp_row * VGA_WIDTH + disp_col);
            int disp_to = 2 * ((disp_row  - 1) * VGA_WIDTH + disp_col);
            vga[disp_to] = vga[disp_from];
            vga[disp_to+1] = vga[disp_from+1];
        }
    }

    //
    // Clear the last line
    //
    for(disp_col = 0; disp_col < VGA_WIDTH; disp_col++)
    {
        int offset = 2 * ((VGA_HEIGHT-1) * VGA_WIDTH + disp_col);
        vga[offset] = ' ';
    }
}

void print_string(const char* str, char color)
{
    while(*str)
    {
        if(*str == '\n')
        {
            cursor_row++;
            cursor_col=0;
        } else 
        {
            put_char(*str, color);
            cursor_col++;
            if(cursor_col >= VGA_WIDTH)
            {
                cursor_row++;
                cursor_col=0;
            }
        }

        if(cursor_row >= VGA_HEIGHT)
        {
            scroll_up();
            cursor_row = VGA_HEIGHT - 1;
            cursor_col = 0;
        }

        move_cursor();
        str++;
    }
}

void kernel_main() {
    for (int i = 0; i < 30; i++) {
        // Compose message
        char msg[40];
        // Print line numbers so you can see scroll
        int len = 0;
        msg[len++] = 'L'; msg[len++] = 'i'; msg[len++] = 'n'; msg[len++] = 'e'; msg[len++] = ' ';
        int n = i, digits = 0, t = i;
        if (n == 0) digits = 1;
        while (t > 0) { digits++; t /= 10; }
        for (int d = digits-1; d >= 0; d--) {
            msg[len + d] = '0' + (n % 10);
            n /= 10;
        }
        len += digits;
        msg[len++] = '\n';
        msg[len] = 0;
        print_string(msg, WHITE_ON_BLACK);
    }
}
/*
void kernel_main() 
{
    print_string("Hello, from Kernel-V!\n to Devjit.\n", RED_ON_WHITE);
}
*/

