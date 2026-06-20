BITS 16
%ifndef ELF_BUILD
[org 0x7e00]
%endif

%include "stage2_kernel.inc"

Start:
    mov dl, 0x80
    mov ah, 0x41
    mov bx, 0x55AA
    int 0x13
    jc NotSupported
    cmp bx, 0xAA55
    jne NotSupported

    ; ReadPacket
    ; si = *ReadPacket
    ; [si]              - to store the size of Read Packet
    ; [si+1]            - Reserved (must be 0)
    ; [si+2] [si+3]     - to store the number of sectors to Read
    ; [si+4] [si+5]     - Offset to Load the Read content
    ; [si+6] [si+7]     - Segment to Load the Read content
    ; [si+8] [si+15]    - LBA to Read from the Disk
    ; 1. Load the Kernel into memory 0x10000 ()
    ; [PMM] Reserved kernel range: 0x65536 - 0x78800 
    ; INT 0x13/AH=42h can only load up to 64KB (127 sectors) per call.
    ; Read the kernel in 127-sector chunks based on generated build size.
LoadKernel:
    mov si, ReadPacket
    mov word [si], 0x10
    mov word [si+4], 0x0000
    mov dword [si+12], 0

    mov word [kernel_load_segment], 0x1000
    mov dword [kernel_next_lba], KERNEL_START_LBA
    mov word [kernel_sectors_left], KERNEL_TOTAL_SECTORS

.read_loop:
    mov ax, [kernel_sectors_left]
    test ax, ax
    jz GetMemoryMap

    cmp ax, 127
    jbe .set_count
    mov ax, 127

.set_count:
    mov word [kernel_chunk_sectors], ax
    mov word [si+2], ax
    mov bx, [kernel_load_segment]
    mov word [si+6], bx
    mov eax, [kernel_next_lba]
    mov dword [si+8], eax
    mov si, ReadPacket

    mov ah, 0x42
    mov dl, 0x80
    int 0x13
    jc ReadError

    ; Advance destination by (count * 512 bytes) => (count * 32) paragraphs.
    mov ax, [kernel_chunk_sectors]
    mov bx, ax
    shl bx, 5
    add word [kernel_load_segment], bx

    sub word [kernel_sectors_left], ax
    xor eax, eax
    mov ax, [kernel_chunk_sectors]
    add dword [kernel_next_lba], eax
    jmp .read_loop

GetMemoryMap:
    xor ax, ax
    xor ebx, ebx
    mov di, 0x5000              ; di = destination index for an entry (16 bit)
    mov es, ax                  ; es = segment where the memory map will be stored 
    xor cx, cx
    mov word [memmap_count], 0

.e820_loop:
    mov eax, 0xe820
    mov edx, 0x534D4150         ; "SMAP" signature
    mov ecx, 24                 ; size of the E820 entry
    int 0x15
    jc .e820_done
    cmp eax, 0x534D4150         ; Check if the signature matches
    jne .e820_done

    add di, 24
    inc word [memmap_count]     ; Increment the memory map count
    test ebx, ebx               ; when ebx != 0 more entries are available
    jnz .e820_loop

.e820_done:
    mov dword   [0x2000], 0x5000
    mov word    ax, [memmap_count]
    mov word    [0x2004], ax

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
memmap_count:   dw 0
kernel_sectors_left: dw 0
kernel_chunk_sectors: dw 0
kernel_load_segment: dw 0
kernel_next_lba:     dd 0

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

