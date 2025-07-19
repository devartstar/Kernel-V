;  ___________________
; |        Free       | -> We will use this region for Protected Mode kernel.
; |___________________| 0x010000
; |    Second-Stage   |
; |      Loader       |
; |___________________| 0x7E00
; |     MBR code      | 
; |___________________| 0x7C00
; |      Free         | -> We used this region for stack.
; |-------------------|
; | BIOS data vectors |
; |-------------------| 0

[BITS 16]
[ORG 0x7c00]

jmp short Start
nop

Start:
	; Set up the segment registers.
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7c00
	sti

LoadStage2:
	mov si, ReadPacket
	mov word[si],   0x10
	mov word[si+2], 0x05
	mov word[si+4], 0x7E00	; Offset in Memory to Load
	mov word[si+6], 0x00	; Segement in Memory to Load
	mov dword[si+8], 0x01	; Read from LBA = 1
	mov dword[si+12], 0x00

	mov ah, 0x42
	int 0x13		; Disk Interrput
	jc ReadError

ReadSuccess:	
	mov ah, 0x13
	mov al, 1
	mov bx, 0xA
	xor dx, dx
	mov bp, MsgSuccess
	mov cx, MsgSuccessL
	int 0x10

	; Transfer control to loaded memory
	jmp 0x7E00
	

NotSupported:
ReadError:
    mov ah, 0x13
    mov al, 1
    mov bx, 0xA
    xor dx, dx
    mov bp, MsgError
    mov cx, MsgErrorL
    int 0x10

End:
	hlt
	jmp End

; Messages to Print
MsgError:	db 'Disk Read Error: Stage II cannot be loaded', 0x0A, 0x0D, 0
MsgErrorL:	equ $-MsgError	
MsgSuccess:	db 'Disk Read Success: Stage II loaded', 0x0A, 0x0D, 0
MsgSuccessL:	equ $-MsgSuccess

; To store the disk address
ReadPacket:	times 16 db 0
; b1 b2 (Packet Size = 16) 
; b3 b4 (Number of Sectors to Read) 
; b5 b6 (Offset to Load in Memory) 
; b7 b8 (Segment to Load in Memory) 
; b9 b10 b11 b12 (32 low bits of LBA to Read) 
; b13 b14 b15 b16 (32 high bits of LBA to Read)

; [0-445] is bootloader coad
times (0x1BE-($-$$)) db 0		; Pad size to 510 bytes

; [446-461] 16 Bytes
db 0x80		; [446] Boot Flag: 0x80
db 0,2,0	; [447-449] CHS Start: C=0 h=0 S=2
db 0x0F		; [450] Partition FAT32
db 0xFF,0xFF,0xFF   ; [451-453] CHS End
dd 1		; [454-457] Starting LBA = 1
dd (29*16*63 - 1)   ; [458-461] Disk Size = 1Mb

; [462-509] 48 Bytes
times (16*3) db 0

; [510-511] 2 Bytes Boot Signature
db 0x55
db 0xAA

