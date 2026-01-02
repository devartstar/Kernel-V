extern isr_common_handler

; Interrupt Service Routines when CPU doesn't push Error Code
%macro ISR_STUB_NOERR 1
global isr_stub_%1

isr_stub_%1:
    ; Push dummy error code for interrupts that don't have one
    push 0
    
    ; Push interrupt number
    push %1
    
    ; Save all registers (pusha pushes: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI)
    pusha
    
    ; Pass pointer to register structure as argument
    mov eax, esp
    push eax
    
    ; Call the common handler
    call isr_common_handler
    
    ; Clean up the stack (remove the pointer argument)
    add esp, 4
    
    ; Restore all registers
    popa
    
    ; Remove interrupt number and error code from stack
    add esp, 8
    
    ; Return from interrupt
    iretd
%endmacro

; Interrupt Service Routine when CPU pushes the Error Code
%macro ISR_STUB_ERR 1
global isr_stub_%1

isr_stub_%1:
    ; CPU already pushed error code, just push interrupt number
    push %1
    
    ; Save all registers (pusha pushes: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI)
    pusha
    
    ; Pass pointer to register structure as argument
    mov eax, esp
    push eax
    
    ; Call the common handler
    call isr_common_handler
    
    ; Clean up the stack (remove the pointer argument)
    add esp, 4
    
    ; Restore all registers
    popa
    
    ; Remove interrupt number and error code from stack
    add esp, 8
    
    ; Return from interrupt
    iretd
%endmacro

; Define which interrupts push error codes and which don't
; Interrupts that push error codes: 8, 10, 11, 12, 13, 14, 17, 21
ISR_STUB_NOERR 0   ; Divide by Zero
ISR_STUB_NOERR 1   ; Debug
ISR_STUB_NOERR 2   ; Non-maskable Interrupt
ISR_STUB_NOERR 3   ; Breakpoint
ISR_STUB_NOERR 4   ; Overflow
ISR_STUB_NOERR 5   ; Bound Range Exceeded
ISR_STUB_NOERR 6   ; Invalid Opcode
ISR_STUB_NOERR 7   ; Device Not Available
ISR_STUB_ERR   8   ; Double Fault (pushes error code)
ISR_STUB_NOERR 9   ; Coprocessor Segment Overrun
ISR_STUB_ERR   10  ; Invalid TSS (pushes error code)
ISR_STUB_ERR   11  ; Segment Not Present (pushes error code)
ISR_STUB_ERR   12  ; Stack Segment Fault (pushes error code)
ISR_STUB_ERR   13  ; General Protection Fault (pushes error code)
ISR_STUB_ERR   14  ; Page Fault (pushes error code)
ISR_STUB_NOERR 15  ; Reserved
ISR_STUB_NOERR 16  ; Floating Point Exception
ISR_STUB_ERR   17  ; Alignment Check (pushes error code)
ISR_STUB_NOERR 18  ; Machine Check
ISR_STUB_NOERR 19  ; SIMD Floating Point Exception
ISR_STUB_NOERR 20  ; Virtualization Exception
ISR_STUB_ERR   21  ; Control Protection Exception (pushes error code)

; Reserved interrupts (22-31)
%assign i 22
%rep 10
ISR_STUB_NOERR i
%assign i i+1
%endrep

; User-defined interrupts (32-255) - typically don't push error codes
%assign i 32
%rep 224
ISR_STUB_NOERR i
%assign i i+1
%endrep