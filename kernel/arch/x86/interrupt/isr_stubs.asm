extern isr_common_handler

%macro ISR_STUB 1
global isr_stub_%1

isr_stub_%1:
	pusha						; pushes edi, esi , ebp, esp, ebx, edx, ecx, eax
	push %1						; interrupt vector number
	push 0						; push error_code
	push esp					; pointer to register/context
	call isr_common_handler
	add esp, 12
	popa
	iretd
%endmacro

; Generate stubs 0...255
%assign i 0
%rep 256
	ISR_STUB i
%assign i i+1
%endrep

