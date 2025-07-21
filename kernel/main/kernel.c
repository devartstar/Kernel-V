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

#define VGA_COLOR(bg, fg)   (((bg) << 0x04) | ((fg) & 0x0f))
#define WHITE_ON_BLACK   VGA_COLOR(VGA_BLACK, VGA_WHITE)
#define RED_ON_WHITE    VGA_COLOR(VGA_WHITE, VGA_RED)
#define BLUE_ON_YELLOW  VGA_COLOR(VGA_YELLOW, VGA_BLUE)

static int cursor_row = 0;
static int cursor_col = 0;

static inline void put_char(char c, char color)
{
    volatile unsigned char* vga = (unsigned char*)VGA_ADDRESS;
    int offset = 2 * (cursor_row * VGA_WIDTH + cursor_col);
    vga[offset] = c;
    vga[offset+1] = color;
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
            cursor_row = VGA_HEIGHT - 1;
        }

        str++;
    }
}

void kernel_main() 
{
    print_string("Hello, from Kernel-V!\n to Devjit.\n", RED_ON_WHITE);
}
