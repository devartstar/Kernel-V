BITS 32
global isr_page_fault
extern page_fault_handler

; ISR (Interrupt Service Routine) for Page Fault
; When the CPU calls the isr_page_fault, it pushes into the stack:
;   EIP         (4 bytes)
;   CS          (4 bytes)
;   EFLAGS      (4 bytes)
;   Error code  (4 bytes)
isr_page_fault:
    cli
    pusha           ; Save all general-purpose registers 
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10    ; point the data segment selector in GDT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax


    ; Then at the beginning of this method call we pusha (eax, ebx, ecx, edx, esi, edi, esp, ebp) - 8 * 4 bytes = 32 bytes
    ; Next we push 4 segment references (ds, es, fs, gs) - 4 * 4 bytes = 16 bytes
    ; The initial push has moved up by 16 + 32 = 48 bytes

    ; ## Stack layout at this point:
    ; push [esp + 48] [error code]      ; [1] pushed my kernel method
    ; [ gs, fs, es, ds ]                ; 16 bytes                      <---- esp
    ; [ edi...eax ]                     ; 32 bytes from pusha
    ; [ error code ]                    ; [2] 4 bytes pushed by CPU     <---- esp + 48
    ; [ eip, cs, eflags, ...]           ; CPU stuff

    push esp                ; Push the current stack pointer as argument to the handler
    call page_fault_handler ; call the page fault handler

    add esp, 4  ; Clean up the stack pointer from the stack
    pop gs
    pop fs
    pop es
    pop ds
    popa  ; Restore all general-purpose registers

    add esp, 4
    sti
    iret

