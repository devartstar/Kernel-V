; Stage 2 Bootloader: Loads the kernel from disk into memory and switches to 32-bit protected mode.

BITS 16
ORG 0x8000              ; Stage 2 loaded at 0x8000 by Stage 1

KERNEL_LBA   equ 2      ; Kernel starts at LBA sector 2
KERNEL_SECTORS equ 16   ; Number of 512-byte sectors to load (change as needed)
KERNEL_SEG   equ 0x1000 ; Segment for 1MB (0x1000 * 16 = 0x10000 = 1MB)
KERNEL_OFF   equ 0x0000 ; Offset within segment

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Load kernel KERNEL_SECTORS sectors from LBA=KERNEL_LBA to 0x100000
    mov si, 0                  ; sector counter (offset)
    mov di, KERNEL_SECTORS     ; total number of sectors

.load_kernel:
    mov ah, 0x02               ; BIOS: read sectors
    mov al, 1                  ; Read 1 sector per call
    mov ch, 0                  ; Cylinder 0
    mov cl, KERNEL_LBA
    add cl, sil                ; cl = sector number (LBA 2 + si)
    mov dh, 0                  ; Head 0
    mov dl, 0x80               ; First hard disk (change to 0x00 for floppy)
    mov bx, KERNEL_OFF         ; Offset = 0
    mov ax, KERNEL_SEG
    add ax, si                 ; Advance segment by si (for each 512-byte sector)
    mov es, ax                 ; ES:BX = load address (0x1000:0, 0x1001:0, ...)
    int 0x13                   ; BIOS disk read

    jc disk_error              ; Jump on error

    inc si                     ; Next sector (sector offset)
    dec di                     ; One less sector to read
    jnz .load_kernel           ; Loop until all sectors loaded

    ; Setup GDT for protected mode
    lgdt [gdt_desc]

    ; Enable protected mode
    mov eax, cr0
    or  al, 1
    mov cr0, eax

    ; Far jump to 32-bit protected mode code
    jmp 0x08:protected_mode_start

disk_error:
    mov si, disk_error_msg
.print_char:
    lodsb
    or al, al
    jz .halt
    mov ah, 0x0E
    int 0x10
    jmp .print_char
.halt:
    cli
    hlt

disk_error_msg db 'Disk error! Halting.', 0

; ============ GDT and Protected Mode Code ===========

[BITS 32]
protected_mode_start:
    mov ax, 0x10               ; Data segment selector in GDT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9FB00           ; Setup stack

    ; Jump to loaded kernel (at 0x100000)
    jmp 0x100000               ; Or: call dword 0x100000

.halt_pm:
    cli
    hlt
    jmp .halt_pm

; ============ GDT structure ===========

align 8
gdt_start:
    dq 0x0000000000000000      ; Null descriptor
    dq 0x00CF9A000000FFFF      ; Code segment: base=0, limit=4GB, 32-bit, readable
    dq 0x00CF92000000FFFF      ; Data segment: base=0, limit=4GB, writable

gdt_desc:
    dw gdt_end - gdt_start - 1 ; Limit
    dd gdt_start               ; Address

gdt_end:

times 4096-($-$$) db 0         ; Pad to 4K, if you want (not required but safe)
