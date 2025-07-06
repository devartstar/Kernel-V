BITS 16
%ifdef BIN
    org 0x8000
%endif

start:
    mov ah, 0x0E
    mov al, 'S'
    int 0x10
    mov al, '2'
    int 0x10

    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    mov bl, 0           ; sector offset (0..15)
    mov cx, 16          ; sector count

.load_kernel:
    mov ah, 0x0E
    mov al, '4'
    int 0x10

    mov ah, 0x02        ; BIOS read sector
    mov al, 1
    mov ch, 0
    mov cl, 9           ; Start at sector 9 (kernel's first sector in image)
    add cl, bl          ; cl = 9 + sector offset
    mov dh, 0
    mov dl, 0x80

    mov ax, bl          ; ax = sector offset
    shl ax, 9           ; ax = offset * 512
    shr ax, 4           ; ax = offset * 32
    add ax, 0x1000      ; es = 0x1000 + (offset * 32)
    mov es, ax
    mov bx, 0

    int 0x13
    jc disk_error

    inc bl
    loop .load_kernel

    mov ah, 0x0E
    mov al, 'L'
    int 0x10

    lgdt [gdt_desc]

    mov ax, cr0
    or  ax, 1
    mov cr0, ax

    jmp 0x08:protected_mode_start

disk_error:
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    cli
    hlt

[BITS 32]
protected_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x9FB00

    mov ah, 0x0E
    mov al, 'P'
    int 0x10

    call dword 0x100000

    mov ah, 0x0E
    mov al, 'K'
    int 0x10

.halt_pm:
    cli
    hlt
    jmp .halt_pm

gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF

gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start
gdt_end:

%ifdef BIN
    times 4096-($-$$) db 0
%endif
