# ==============================================================================
# GDB Configuration for Kernel-V Debugging
# ==============================================================================

# Connect to QEMU
target remote :1234

# Set architecture  
set architecture i386

# Load symbols
symbol-file build/kernel/kernel.elf
add-symbol-file build/bootloader/stage1.elf 0x7c00
add-symbol-file build/bootloader/stage2.elf 0x7e00

# Enable TUI
tui enable
layout split
focus cmd

# Set useful breakpoints
break kernel_main
break panic

# Custom commands
define reload-symbols
    symbol-file build/kernel/kernel.elf
    add-symbol-file build/bootloader/stage1.elf 0x7c00  
    add-symbol-file build/bootloader/stage2.elf 0x7e00
end

define kernel-bt
    info registers
    bt
    frame
    list
end

echo \n
echo ==== Kernel-V Debug Session ====\n
echo Available commands:\n
echo   reload-symbols  - Reload all symbol files\n
echo   kernel-bt      - Show kernel backtrace with context\n
echo ================================\n
echo Ready to debug. Use 'continue' to start.\n