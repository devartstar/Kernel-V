BITS 32

global _start       ; export this label for bootloader to know the kernel entry point
extern kernel_main

_start:
    mov esp, 0x9FB00
    call kernel_main

global switch_to_high_stack

switch_to_high_stack:
    mov eax, [esp+8]      ; function pointer
    mov esp, [esp+4]      ; set new stack pointer (which already has a fake return addr)
    jmp eax               ; JMP, instead of CALL, so that we don't push return addr

.hang:
    cli
    hlt
    jmp .hang
