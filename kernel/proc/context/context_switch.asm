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

	; Save current EFLAGS - ensure interrupts are enabled before saving
	pushfd
	pop ecx
	; Force IF bit (bit 9) to be set in saved EFLAGS to ensure proper restoration
	or ecx, 0x200
	mov [eax + PCBCTX_EFLAGS_OFFSET], ecx

	; store the eip, after context siwtch back to this proc - execute from here
	; [esp] pointer to return address
	mov ecx, [esp]
	mov [eax + PCBCTX_EIP_OFFSET], ecx

	; store the esp offset after return
	lea ecx, [esp+4]
	mov [eax + PCBCTX_ESP_OFFSET], ecx

	; Load context from next->context
	mov eax, [esp+8]
	
	; Restore EFLAGS first (ensure IF bit is set)
	mov edx, [eax + PCBCTX_EFLAGS_OFFSET]
	; Force IF bit (bit 9) to be set to ensure interrupts are enabled
	or edx, 0x200
	push edx
	popfd
	
	; Now restore all general-purpose registers
	mov ebx, [eax + PCBCTX_EBX_OFFSET]
	mov ecx, [eax + PCBCTX_ECX_OFFSET]
	mov edx, [eax + PCBCTX_EDX_OFFSET]
	mov esi, [eax + PCBCTX_ESI_OFFSET]
	mov edi, [eax + PCBCTX_EDI_OFFSET]
	mov ebp, [eax + PCBCTX_EBP_OFFSET]

	; Use EDX for EFLAGS (avoid ECX collision)
	mov edx, [eax + PCBCTX_EFLAGS_OFFSET]
	push edx
	popfd

	mov esp, [eax + PCBCTX_ESP_OFFSET]

	; Use EDX for EIP too (ECX is now loaded with process context)
	mov edx, [eax + PCBCTX_EIP_OFFSET]
	jmp edx
	; Use EDX for EIP too (ECX is now loaded with process context)
	mov edx, [eax + PCBCTX_EIP_OFFSET]
	jmp edx
