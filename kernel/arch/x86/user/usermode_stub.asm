; arch/x86/usermode_stub.asm
BITS 32

global usermode_stub
global usermode_stub_end

usermode_stub:
.loop:
    mov eax, 0              ; Test syscall number
    mov ebx, 0x12345678     ; test args
    int 0x80                ; Syscall interrupt
    jmp .loop               ; Infinite loop of syscalls

usermode_stub_end:

