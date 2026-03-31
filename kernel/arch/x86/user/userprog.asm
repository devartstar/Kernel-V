; arch/x86/user/userprog.asm
BITS 32
USER_BASE equ 0x00400000

section .data
msg:	db "Hello from user mode!", 0xA
msg_len equ $-msg

section .text
; Make entry point local to avoid conflicts with kernel's _start
user_entry:
	; write (fd=1, buf=msg, len=msg_len)
.loop
	mov eax, 2	; SYS_WRITE
	mov ebx, 1	; fd=1 => stdout
	mov ecx, USER_BASE + msg
	mov edx, msg_len
	int 0x80

	; getpid()
	mov eax, 3	; SYS_GETPID
	int 0x80	; pid returned in eax

	; exit()
	mov eax, 1	; SYS_EXIT
	mov ebx, 0
	int 0x80

	jmp .loop
