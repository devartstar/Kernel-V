# ==============================================================================
# GDB Configuration for Stage1 Bootloader Debugging
# ==============================================================================

# Connect to QEMU
target remote :1234

# Set 16-bit real mode architecture
set architecture i8086

# Load Stage1 symbols
add-symbol-file build/bootloader/stage1.elf 0x7c00

# Enable TUI
tui enable
layout asm
focus cmd

# Set breakpoints
hbreak *0x7c00

# Show info
info breakpoints
info registers

echo \n
echo ==== Stage1 Bootloader Debug ====\n
echo Stage1 loaded at 0x7c00\n  
echo Use 'stepi' to step through instructions\n
echo Use 'continue' to run to next breakpoint\n
echo =================================\n