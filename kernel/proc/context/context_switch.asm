%include "kernel/include/proc/proc_offset_asm.h"

global switch_to
section .text

; void switch_to (pcb_t *prev, pcb_t *next)
switch_to:
	; Arguments :	[esp]	= Return pointer
	;				[esp+4] = prev
	;				[esp+8] = next

	mov eax, [esp+4]
	; Save context to prev->context
	mov [eax + PCBCTX_EBX_OFFSET], ebx
	mov [eax + PCBCTX_ECX_OFFSET], ecx
	mov [eax + PCBCTX_EDX_OFFSET], edx
	mov [eax + PCBCTX_ESI_OFFSET], esi
	mov [eax + PCBCTX_EDI_OFFSET], edi
	mov [eax + PCBCTX_EBP_OFFSET], ebp

	; Save EFLAGS - force IF=1 so process always resumes with interrupts enabled
	; When called from a timer ISR, CPU has cleared IF. We must NOT save IF=0
	; because restoring it would disable interrupts and cause re-entrant timer nesting.
	; pushfd
	; pop ecx
	; or ecx, 0x200
	; mov [eax + PCBCTX_EFLAGS_OFFSET], ecx

	; Save EIP (return address on stack)
	mov ecx, [esp]
	mov [eax + PCBCTX_EIP_OFFSET], ecx

	; Save ESP (caller's stack pointer, above return address)
	lea ecx, [esp+4]
	mov [eax + PCBCTX_ESP_OFFSET], ecx

	; ---- Restore next process context ----
	mov eax, [esp+8]

	; Restore general-purpose registers
	mov ebx, [eax + PCBCTX_EBX_OFFSET]
	mov ecx, [eax + PCBCTX_ECX_OFFSET]
	mov edx, [eax + PCBCTX_EDX_OFFSET]
	mov esi, [eax + PCBCTX_ESI_OFFSET]
	mov edi, [eax + PCBCTX_EDI_OFFSET]
	mov ebp, [eax + PCBCTX_EBP_OFFSET]

	; Switch to next process's stack
	mov esp, [eax + PCBCTX_ESP_OFFSET]

	; Restore EFLAGS on the NEW stack (balanced push/popfd)
	; push dword [eax + PCBCTX_EFLAGS_OFFSET]
	; popfd

	; Load EIP into eax (last use of PCB pointer) and jump
	mov eax, [eax + PCBCTX_EIP_OFFSET]
	jmp eax
