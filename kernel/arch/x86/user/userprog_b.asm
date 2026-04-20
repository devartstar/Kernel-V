BITS 32
global _start

section .text
_start:
    mov eax, 2          ; SYS_write
    mov ebx, 1
    mov ecx, msg
    mov edx, msg_len
    int 0x80

    mov eax, 3          ; SYS_getpid
    int 0x80

    mov eax, 1          ; SYS_exit
    xor ebx, ebx
    int 0x80

.hang:
    jmp .hang

section .data
msg db "Hello from user B", 10
msg_len equ $ - msg
