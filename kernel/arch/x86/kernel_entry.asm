BITS 32

global _start       ; export this label for bootloader to know the kernel entry point
extern kernel_main

_start:
    mov esp, 0x9FB00
    call kernel_main

global switch_to_high_stack

switch_to_high_stack:
    mov esp, [esp+4]        ; get new_esp from stack argument
    jmp [esp+8]             ; jump to function pointer argument

.hang:
    cli
    hlt
    jmp .hang
