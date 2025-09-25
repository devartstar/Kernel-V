BITS 32
SECTION .text

global double_fault_handler

double_fault_handler:
    ; Disable interrupts
    cli
    
    ; Set up segments (in case they're corrupted)
    mov ax, 0x10        ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Print debug message to screen (VGA text mode at 0xB8000)
    mov edi, 0xB8000    ; VGA text buffer
    mov esi, df_msg     ; Message string
    mov ah, 0x4F        ; White text on red background
    mov ecx, df_msg_len ; Message length
    
.print_loop:
    lodsb               ; Load byte from [esi] into al
    test al, al         ; Check for null terminator
    jz .print_done
    stosw               ; Store ax (al + ah) to [edi]
    loop .print_loop
    
.print_done:
    ; Infinite loop with halt
.halt_loop:
    hlt
    jmp .halt_loop

SECTION .data
df_msg: db "DOUBLE FAULT! System halted.", 0
df_msg_len equ $ - df_msg - 1
