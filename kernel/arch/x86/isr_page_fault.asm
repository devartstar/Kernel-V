[BITs 32]
[GLOBAL isr_page_fault]

; The CPU pushes (in order): error code, eip, cs, eflags, esp (if privilege change)
isr_page_fault:
    cli
    pusha  ; Save all general-purpose registers 
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10  ; point the data segment selector in GDT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; ## Info:
    ; When the CPU calls the ISR_page_fault, it pushes into the stack:
    ; EIP (4 bytes)
    ; CS (4 bytes)
    ; EFLAGS (4 bytes)
    ; Error code (4 bytes)
    ; Then at the beginning of this method call we pusha (eax, ebx, ecx, edx, esi, edi, esp, ebp) - 8 * 4 bytes = 32 bytes
    ; Next we push 4 segment references (ds, es, fs, gs) - 4 * 4 bytes = 16 bytes
    ; The initial push has moved up by 16 + 32 = 48 bytes

    ; ## Stack layout at this point:
    ; push [esp + 48] [error code]      ; [1] pushed my kernel method
    ; [ gs, fs, es, ds ]                ; 16 bytes                      <---- esp
    ; [ edi...eax ]                     ; 32 bytes from pusha
    ; [ error code ]                    ; [2] 4 bytes pushed by CPU     <---- esp + 48
    ; [ eip, cs, eflags, ...]           ; CPU stuff

    push dword [esp + 48]  ; Push the error code onto the stack
    call page_fault_handler ; call the page fault handler

    add esp, 4  ; Clean up the error code from the stack
    pop gs
    pop fs
    pop es
    pop ds
    popa  ; Restore all general-purpose registers

    add esp, 4
    sti
    iret

