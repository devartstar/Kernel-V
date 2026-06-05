BITS 32
org 0x00400000
global _start

section .text
_start:
    mov esi, 5          ; loop counter

.loop:
    ; SYS_write(fd=1, buf=msg, len=msg_len)
    mov eax, 2
    mov ebx, 1
    mov ecx, msg
    mov edx, msg_len
    int 0x80

    ; SYS_getpid()
    mov eax, 3
    int 0x80

    ; SYS_sched_yield() - let other processes run
    mov eax, 4
    int 0x80

    dec esi
    jnz .loop

    ; SYS_exit(0)
    mov eax, 1
    xor ebx, ebx
    int 0x80

.hang:
    jmp .hang

section .data
msg db "Hello from user B", 10
msg_len equ $ - msg
