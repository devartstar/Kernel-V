BITS 16
ORG 0x8000

start:
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7c00		; Stack pointer to point to any safe 
				; known and free area below 0x8000
				; Why 0x7c00 works ?
				; 0x7c00 used by stage 1 but once it handed off
				; control to stage 2 we can use that location

	; Load Kernel from LBA = 2, 16 sectors
	; Load to 0x100000 (1Mb)
	mov si, 0		; offset of sectors to read
	mov dx, 16		; Sector Offset (foctor for 512)

.load_kernel:
	push si
	mov ah, 0x02		; Read Operation
	mov al, 0x01		; 1 sector at a time

	; Read From Location
	mov ch, 0x00		; Cylinder = 0
	mov cl, 0x02		; Sector = 2

	push ax
	mov ax, si
	add cl, al		; Add the sector offset
	pop ax

	mov dh, 0
	mov dl, 0x80
	
	mov es, word [KERN_SEG]
	mov bx, si
	shl bx, 9		; bx = di * 512(2^9)

	int 0x13
	jc disk_error

	inc si
	dec dx			; loop till si = 16 -> 0
	jnz .load_kernel

	; Setup GDT for protected mode
	; lgdt = Load Global Descriptor Table
	lgdt [gdt_desc]

	; Enable Protected Mode
	; CR0 - is a CPU control Register.
	;	One of its bits (bit 0) is the PE bit.
	mov eax, cr0
	or al, 1
	mov cr0, eax

	; Far Jump to 32 bit code
	; Far Jump - helps to change both CS and IP
	; jmp segment:offser
	jmp 0x08:protected_mode_start

disk_error:
	cli
	hlt

[BITS 32]
protected_mode_start:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov esp, 0x9FB00	; set up the stack
	; physical stack address = base of ss descriptor + esp

	call dword 0x100000	; jump to the kernel

	hlt
	jmp $

KERN_SEG dw 0x1000

; GDT (flat, minimal)
gdt_start:
	; each entry is 8 bytes
	dq 0x0000000000000000
	dq 0x00CF9A000000FFFF ; Code segment
	dq 0x00CF92000000FFFF ; Data segment

gdt_desc:
	dw gdt_desc_end - gdt_start - 1		; Limits
	dd gdt_start				; Base

gdt_desc_end:

times 4096-($-$$) db 0		; Fill upto 4K if needed
