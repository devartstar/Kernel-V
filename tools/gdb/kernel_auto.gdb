set architecture i386
target remote :1234
symbol-file build/kernel/kernel.elf
add-symbol-file build/bootloader/stage1.elf 0x7c00
add-symbol-file build/bootloader/stage2.elf 0x7e00
# Set source directories
directory .
directory kernel/
directory kernel/core/init/
directory kernel/core/debug/
directory kernel/drivers/video/
# Start with assembly view
tui enable
layout asm
focus cmd
# Set breakpoint and auto-continue to C code
break kernel_main
echo === Auto-continuing to C source code ===
continue
# Switch to source view when we reach C code
layout split
refresh
list
echo
echo === Now at C source code! ===
echo Commands: step, next, continue, bt, list
echo Use Ctrl+X+A to toggle TUI mode
