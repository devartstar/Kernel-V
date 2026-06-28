BITS 32
org 0x00400000
global _start

; -----------------------------------------------------------------------------
; Self-checking syscall integration program.
;
; Exercises real syscalls and verifies their return values (in eax) from user
; mode. Reports the verdict through SYS_EXIT:
;
;   exit 0 -> all checks passed
;   exit 1 -> SYS_GETPID returned a negative pid
;   exit 2 -> SYS_WRITE to stdout did not return the byte count
;   exit 3 -> SYS_WRITE with an invalid fd did not return -1
;
; The kernel-side harness (syscall_itest.c) spawns this blob and asserts that it
; terminates with exit code 0.
; -----------------------------------------------------------------------------

%define SYS_EXIT        1
%define SYS_WRITE       2
%define SYS_GETPID      3
%define SYS_SCHED_YIELD 4

section .text
_start:
    ; --- check 1: getpid returns a non-negative pid ---
    mov eax, SYS_GETPID
    int 0x80
    test eax, eax
    js   .fail_getpid           ; sign bit set => negative => fail

    ; let other processes interleave to exercise the scheduler path
    mov eax, SYS_SCHED_YIELD
    int 0x80

    ; --- check 2: write to stdout returns the number of bytes written ---
    mov eax, SYS_WRITE
    mov ebx, 1                  ; fd = stdout
    mov ecx, msg
    mov edx, msg_len
    int 0x80
    cmp eax, msg_len
    jne  .fail_write

    ; --- check 3: write to an invalid fd is rejected with -1 ---
    mov eax, SYS_WRITE
    mov ebx, 7                  ; invalid fd (not stdout)
    mov ecx, msg
    mov edx, msg_len
    int 0x80
    cmp eax, -1
    jne  .fail_badfd

    ; --- all checks passed ---
    mov eax, SYS_EXIT
    xor ebx, ebx                ; exit code 0 = success
    int 0x80
    jmp .hang

.fail_getpid:
    mov eax, SYS_EXIT
    mov ebx, 1
    int 0x80
    jmp .hang

.fail_write:
    mov eax, SYS_EXIT
    mov ebx, 2
    int 0x80
    jmp .hang

.fail_badfd:
    mov eax, SYS_EXIT
    mov ebx, 3
    int 0x80
    jmp .hang

.hang:
    jmp .hang

section .data
msg db "syscall selftest", 10
msg_len equ $ - msg
