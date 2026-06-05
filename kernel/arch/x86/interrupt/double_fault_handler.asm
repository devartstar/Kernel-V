; filepath: /home/devch/Projects/Kernel-V/kernel/arch/x86/double_fault_handler.asm
BITS 32
SECTION .text

global double_fault_handler

double_fault_handler:
    ; Disable interrupts immediately
    cli
    
    ; Write multiple magic values to different memory locations for debugging
    mov eax, 0xDEADBEEF
    mov [0x15000], eax     ; Magic value 1
    
    mov eax, 0xCAFEBABE  
    mov [0x15004], eax     ; Magic value 2
    
    mov eax, 0x12345678
    mov [0x15008], eax     ; Magic value 3
    
    ; Try to write to VGA memory as well (since we know it's mapped)
    mov word [0xB8000], 0x4F44  ; 'D' with white on red
    mov word [0xB8002], 0x4F46  ; 'F' with white on red
    
    ; Safe infinite loop
.safe_halt:
    hlt
    jmp .safe_halt