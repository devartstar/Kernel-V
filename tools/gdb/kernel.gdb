# ==============================================================================
# GDB Configuration for Kernel-V Debugging
# ==============================================================================

# Connect to QEMU
target remote :1234

# Set architecture  
set architecture i386

# Set source directories (both relative and absolute)
directory .
directory /home/devch/Projects/Kernel-V/
directory /home/devch/Projects/Kernel-V/kernel/
directory /home/devch/Projects/Kernel-V/kernel/core/init/
directory /home/devch/Projects/Kernel-V/kernel/core/debug/
directory /home/devch/Projects/Kernel-V/kernel/core/panik/
directory /home/devch/Projects/Kernel-V/kernel/drivers/video/
directory /home/devch/Projects/Kernel-V/kernel/lib/printf/
directory /home/devch/Projects/Kernel-V/kernel/lib/string/
directory /home/devch/Projects/Kernel-V/kernel/mm/
directory /home/devch/Projects/Kernel-V/kernel/proc/
directory /home/devch/Projects/Kernel-V/kernel/arch/x86/

# Load symbols
symbol-file build/kernel/kernel.elf
add-symbol-file build/bootloader/stage1.elf 0x7c00
add-symbol-file build/bootloader/stage2.elf 0x7e00

# Enable TUI but start with assembly (since we're in asm code initially)
tui enable
layout asm
focus cmd

# Set useful breakpoints
break kernel_main
break high_stack_entry
break panic

# Auto-continue to kernel_main and switch to source view
define auto-start
    continue
    if $_thread != 0
        layout split
        refresh
        list
        echo \n=== Now at C source code ===\n
    end
end

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

define show-source
    layout split
    refresh
    list
end

define goto-main
    continue
    layout split
    list
end

define debug-info
    echo ==== DEBUG INFO ====\n
    info files
    info sources
    echo Current function: 
    info function
    echo ===================\n
end

echo \n
echo ==== Kernel-V Debug Session ====\n
echo Available commands:\n
echo   auto-start      - Continue to kernel_main and show source\n
echo   goto-main       - Jump to kernel_main with source view\n
echo   reload-symbols  - Reload all symbol files\n
echo   kernel-bt       - Show kernel backtrace with context\n
echo   show-source     - Switch to source view\n
echo   debug-info      - Show debug information\n
echo ================================\n
echo You're currently in assembly code.\n
echo Type 'auto-start' or 'goto-main' to reach C source.\n
echo Or just 'continue' to proceed manually.\n