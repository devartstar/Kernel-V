; Minimal user mode stub which triggers an int 0x80, then hlts in loop
BITS 32

global usermode_stub

usermode_stub:
	mov eax, 0x12345678
	int 0x80

.halt:
	hlt
	jmp .halt
