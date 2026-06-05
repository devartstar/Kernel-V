# Connect to QEMU
target remote :1234
# Set 16-bit real mode architecture
set architecture i8086
# Load symbol files
add-symbol-file build/bootloader/stage1.elf 0x7c00
add-symbol-file build/bootloader/stage2.elf 0x7e00
# Enable TUI mode with layout
tui enable
layout asm
focus cmd
# Set breakpoints at key locations
hbreak *0x7c00
hbreak *0x7e00
hbreak *0x10000
# Display breakpoints and symbols
info breakpoints
info files
# Show current state
info registers
x/5i $pc
# Debugging tips
echo
echo ==== Bootloader Debug Session ====
echo Stage1 starts at 0x7c00
echo Stage2 starts at 0x7e00
echo Kernel starts at 0x10000
echo Use 'continue' to run to first breakpoint
echo Use 'stepi' to step one instruction
echo Use 'info registers' to see register state
echo ====================================
