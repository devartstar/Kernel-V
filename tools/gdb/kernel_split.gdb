set architecture i386
target remote :1234
symbol-file build/kernel/kernel.elf
# Enable TUI with split layout (source + assembly)
tui enable
layout split
focus cmd
break kernel_main
info breakpoints
