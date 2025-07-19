BITS 32

global _start       ; export this label for bootloader to know the kernel entry point
extern kernel_main

_start:
    mov esp, 0x9FB00
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
