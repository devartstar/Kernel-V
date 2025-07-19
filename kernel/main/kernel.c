void main() 
{
    char *vga = (char *)0xb8000;
    vga[0] = 'h';
    vga[1] = 0x07;
    vga[2] = 'i';
    vga[3] = 0x07;
    vga[4] = '_';
    vga[5] = 0x07;
    vga[6] = 'd';
    vga[7] = 0x07;
    vga[8] = 'e';
    vga[9] = 0x07;
    vga[10] = 'v';
    vga[11] = 0x07;
    for (;;) {}
}
