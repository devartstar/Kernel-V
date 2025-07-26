BITS 16
%ifndef ELF_BUILD
[org 0x7e00]
%endif

Start:
    mov dl, 0x80
    mov ah, 0x41
    mov bx, 0x55AA
    int 0x13
    jc NotSupported
    cmp bx, 0xAA55
    jne NotSupported


    ; 1. Load the Kernel
LoadKernel:
    mov si, ReadPacket
    mov word[si], 0x10
    mov word[si+2], 0x08        ; Load 8 sectors from the Disk (4096 bytes)
    mov word[si+4], 0x00
    mov word[si+6], 0x1000      ; Segment to Load to Load
    mov dword[si+8], 0x09        ; Read from the 10th Sector (LBA=9, offset 0x1200)
    mov dword[si+12], 0x00

    mov ah, 0x42
    int 0x13
    jc ReadError

SetVideoMode:
    mov ax, 0x03
    int 0x10

SwitchToProtectedMode:
    cli
    lgdt [GDT32Pointer]         ; Load Global Descriptor Table
    lidt [IDT32Pointer]         ; Load Invalid IDT

    mov eax, cr0
    or eax, 0x01
    mov cr0, eax

    jmp 0x08:PMEntry


NotSupported:
    mov ah, 0x13
    mov al, 1
    mov bx, 0x0A
    xor dx, dx
    mov bp, MsgNoSupport
    mov cx, MsgNoSupportL
    int 0x10
    jmp End

ReadError:
    mov ah, 0x13
    mov al, 1
    mov bx, 0x0A
    xor dx, dx
    mov bp, MsgError
    mov cx, MsgErrorL
    int 0x10

End:
    hlt
    jmp End


MsgError:       db "Cannot Load Kernel", 0x0A, 0x0D, 0
MsgErrorL:      equ $-MsgError
MsgSuccess:     db "Successfully Loaded Kernel", 0x0A, 0x0D, 0
MsgSuccessL:    equ $-MsgSuccess
MsgNoSupport:   db "LBA extension support check failed", 0x0A, 0x0D, 0
MsgNoSupportL:  equ $-MsgNoSupport


ReadPacket:     times 16 db 0

; Global Descriptor Table
GDT32:
    dq 0                ; First entry (8 bytes) is always null

CodeSegDes32:
    dw 0xFFFF           ; [0-1] Segment Size - set to max
    db 0, 0, 0          ; [2-4] Lower 24 bits of base address
                        ; 0 - code segment starts from 0

    db 0b10011010       ; [5-5] segment attributes
                        ; P=1, DPL=00, S=1, TYPE=1010

    db 0b11001111       ; [6-6] segment size and attributes
                        ; G=1(4Kb Granularity) D=1(32 bit protected) L=0 (not 64 bit code) 
                        ; A=0(Availability for Software use) LIMIT=1111 (upper bits of segment size)

    db 0                ; [7-7] upper 8 bits of base address
                        ; code segment start from 0

DataSegment32:
    dw 0xFFFF
    db 0, 0, 0
    db 0b10010010       ; TYPE=0010 - Writable Segment
    db 0b11001111
    db 0

GDT32Len:       equ $-GDT32

GDT32Pointer:   dw GDT32Len - 1         ; Length of GDT
                dd GDT32                ; Address of GDT

IDT32Pointer:   dw 0
                dd 0

[BITS 32]
PMEntry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov gs, ax
    mov fs, ax
    mov ss, ax

    ; mov esp, 0x7c00     ; kernel entry will set up stack pointer

    jmp 0x08:0x10000
    jmp $

