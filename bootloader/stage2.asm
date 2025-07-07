BITS 16
%ifdef BIN
    org 0x0000
%endif

start:
    ; Print 'S2' at start
    mov ah, 0x0E
    mov al, 'S'
    int 0x10
    mov al, '2'
    int 0x10

    ; Set up segment registers
    cli
    mov ax, 0x0600           ; This must match the segment where Stage2 was loaded
    mov ds, ax
    mov es, ax
    sti
    xor ax, ax
    mov ss, ax
    mov sp, 0x7c00

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
    mov bx, si
    add cl, bl          ; cl = 9 + offset (sectors 9..24)
	
    call enable_a20

    mov ax, 0x1000
    mov es, ax
    mov bx, si
    shl bx, 9           ; bx = si * 512

    int 0x13
    jc disk_error

    inc si
    cmp cl, 25
    jl .load_kernel

    ; Print 'L' after kernel load
    mov ah, 0x0E
    mov al, 'L'
    int 0x10

    ; Setup GDT for protected mode
    ; Patch GDT base to physical address (0x6000 + offset of gdt_start)

    ; loads the value of mem addr where gdt to start into eax
    mov eax, 0x7000
    add eax, gdt_start
    mov [gdt_desc+2], eax

    lgdt [gdt_desc]

    cli
    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    db 0x66
    jmp dword 0x08:protected_mode_start

enable_a20:
    in al, 0x92
    or al, 0x02
    out 0x92, al
    ret

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

; 1 entry in gdt - 8 bytes - 64 bits
; segment limit 20 bits - 
; 	i. 	16 bits start of the desc 
;	ii. 	4 bits are in the flags serge
;	how large the segment is.
;	granularity flag - bytes or 4kb blocks
; base addrr 
;	32 bits where the seg starts in memory
;	i. 	16 bits - after limits in beginning 
;	ii. 	8 bits - middle before access and flag field
;	iii. 	8 bits - at the end
; base addr + limit -> helps CPU to figure 
;			exact memory range
; access bytes
;	8 bit field - what kind of segment and how used
;	(bit no. 7) present bit - 1 segmnent present
;	(5-6) desc privileg level - level to access the segment - 0 for kernel
;	(4) desc type - if segment is (0)system or (1)code/data seg
;	(0-3) type bit - 4 bits - more detials about the segment
; Flags
;	4 bits - additional info about how segment should be handeled by CPU
;	granularity - scale of segment limit - (0)limit in bytes (1)4kb blocks
;	default operation size bit (db) - segment is for (0)16/(1)32 bits 
;	long mode - 1 for 64 bit segment, 0 for 32 bit real mode 
;	available - reserved for system software use. Not used now


gdt_start:
    dq 0x0000000000000000	
    ; null descripter - all gdt start

    dq 0x00CF9A000000FFFF	
    ; descriptor for - code seg
    ; base addr = 0 (0x000)
    ; limit = FFFF
    ; Flags - CF
    ; access byte - 9A (ring 0 priv)

    dq 0x00CF92000000FFFF
    ; data segment
    ; base addr = 0
    ; limit = FFFF
    ; access byte = 92 
    ; Flags = CF

; gdt descriptor structure - is used by lgdt
; tells the cpu where gdt present in memory and how large
gdt_desc:
    dw gdt_end - gdt_start - 1		; size /limit
    dd gdt_start			; where gdt starts
gdt_end:

%ifdef BIN
    times 4096-($-$$) db 0
%endif
