# ==============================================================================
# GDB Configuration for Stage2 Bootloader Debugging  
# ==============================================================================

# Connect to QEMU
target remote :1234

# Set architecture
set architecture i8086

# Load Stage2 symbols
add-symbol-file build/bootloader/stage1.elf 0x7c00
add-symbol-file build/bootloader/stage2.elf 0x7e00

# Enable TUI
tui enable
layout asm
focus cmd

# Set breakpoints
hbreak *0x7c00
hbreak *0x7e00
hbreak *0x10000

# Show info
info breakpoints

echo \n
echo ==== Stage2 Bootloader Debug ====\n
echo Stage1: 0x7c00, Stage2: 0x7e00\n
echo Kernel will be loaded at 0x10000\n
echo Use 'continue' to proceed through stages\n
echo =================================\n