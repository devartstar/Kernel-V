; arch/x86/usermode_stub.asm
BITS 32

global usermode_stub
global usermode_stub_end

usermode_stub:
    mov eax, 0x12345678
    int 0x80

.halt:
    hlt
    jmp .halt

usermode_stub_end:

