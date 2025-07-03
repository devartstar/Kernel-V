; Kernel Entry point
BITS 32
GLOBAL _start		; start symbol should be visible to the linker
EXTERN kernel_main	; ref. to the c function

_start:
	; optioanl: set up stack if bootloader didn't
	mov esp, 0x9FB00
	call kernel_main	; call the C kernel main function

.hang:
	cli
	hlt
	jmp .hang
