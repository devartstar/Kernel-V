BITS 16
ORG 0x7c00

start:
	; Set up the segment registers.
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7c00

	; Reading from: 	1 sector from the disk - LBA = 1, CHS = (0,0,2)
	; Reading content: 	Bootloader stage2
	; CopyTo Location: 	0x8000:0000
	mov ah, 0x02		; BIOS Read Sector Function
	mov al, 0x01		; # of sectors to read
	
	mov ch, 0x00		; Cylinder 0
	mov cl, 0x02		; Sector 2 (MBR is Sector 1)
	mov dh, 0x00		; Head 0

	; dl - stores the drive to operate on
	; BIOS loads the first sector - dl contains the drive number 
	; 0x80 for HDD
	; 0x00 for Floppy
	; this value is stored in a MEMORY LABEL
	mov dl, [BOOT_DRIVE]
	
	mov bx, 0x8000		; ES:BX = 0x0000:0x8000
	mov ax, 0x0000
	mov es, ax

	int 0x13		; BIOS interrupt for Disk Read

	jc disk_error		; if any error reading - carry bit is set
	jmp 0x0000:0x8000	; No error - jimp to loaded stage 2

disk_error:
	mov si, msg_error
.print_char:
	lodsb
	or al, al
	jz .halt
	mov ah, 0x0E		; Print character
	int 0x10
	jmp .print_char

.halt:
	cli
	hlt

msg_error:	db 'Disk Read Error', 0x0A, 0x0D, 0

times 510-($-$$) db 0		; Pad size to 510 bytes
dw 0xAA55			; Boot signature in (511-512 Bytes)

BOOT_DRIVE equ 0x7c00+0x24

