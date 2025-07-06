BITS 16
%ifdef BIN
    org 0x8000
%endif

start:
    ; Print 'S2' at start
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

    mov cx, 16          ; Number of kernel sectors to load
    mov si, 0           ; Sector offset (0..15)

.load_kernel:
    ; Print '4' each sector read
    mov ah, 0x0E
    mov al, '4'
    int 0x10

    mov ah, 0x02        ; BIOS: read sector(s)
    mov al, 1
    mov ch, 0           ; Cylinder 0
    mov dh, 0           ; Head 0
    mov dl, 0x80        ; First hard disk

    mov cl, 9           ; Sector 9 is first kernel sector
    add cl, si          ; cl = 9 + offset (sectors 9..24)

    mov ax, 0x1000
    mov es, ax
    mov bx, si
    shl bx, 9           ; bx = si * 512

    int 0x13
    jc disk_error

    inc si
    dec cx
    jnz .load_kernel

    ; Print 'L' after kernel load
    mov ah, 0x0E
    mov al, 'L'
    int 0x10

    ; Setup GDT for protected mode
    lgdt [gdt_desc]

    cli
    mov eax, cr0
    or  eax, 1
    mov cr0, eax
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

    ; Print 'P' in protected mode
    mov byte [0xB8000], 'P'

    call dword 0x100000

    ; Print 'K' after kernel returns
    mov byte [0xB8002], 'K'

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
