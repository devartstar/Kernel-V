; filepath: /home/devch/Projects/Kernel-V/kernel/arch/x86/double_fault_handler.asm
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
    
    ; Clear screen first
    mov edi, 0xB8000    ; VGA text buffer
    mov ecx, 80*25      ; 80x25 screen
    mov ax, 0x4F20      ; White space on red background
    rep stosw
    
    ; Print debug message to screen (VGA text mode at 0xB8000)
    mov edi, 0xB8000    ; VGA text buffer start
    mov esi, df_msg     ; Message string
    mov ah, 0x4F        ; White text on red background
    
.print_loop:
    lodsb               ; Load byte from [esi] into al
    test al, al         ; Check for null terminator
    jz .print_done
    stosw               ; Store ax (al + ah) to [edi]
    jmp .print_loop
    
.print_done:
    ; Print stack overflow specific message
    mov edi, 0xB8000 + (2*80*2) ; Second line
    mov esi, stack_msg
    mov ah, 0x4E        ; Yellow text on red background
    
.print_stack_loop:
    lodsb
    test al, al
    jz .halt_loop
    stosw
    jmp .print_stack_loop

    ; Infinite loop with halt
.halt_loop:
    hlt
    jmp .halt_loop

SECTION .data
df_msg: db "DOUBLE FAULT! System halted.", 0
stack_msg: db "Stack overflow detected! Check guard page mapping.", 0
