set architecture i386
target remote :1234
symbol-file build/kernel/kernel.elf
# Enable TUI mode with source layout
tui enable
layout src
focus cmd
break kernel_main
# Show breakpoints
info breakpoints
# Ready to debug - use 'continue' to start
