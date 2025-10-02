%include "kernel/include/proc_offset_asm.h"

global switch_to
section .text

; void switch_to (pcb_t *prev, pcb_t *next)
switch_to:
	; Arguments :	[esp]	= Return pointer
	;				[esp+4] = prev
	;				[esp+8] = next
	mov eax, [esp+4]
	mov edx, [esp+8]

	; Save context to prev->context
	mov [eax + PCBCTX_EBX_OFFSET], ebx
	mov [eax + PCBCTX_ECX_OFFSET], ecx
	mov [eax + PCBCTX_EDX_OFFSET], edx
	mov [eax + PCBCTX_ESI_OFFSET], esi
	mov [eax + PCBCTX_EDI_OFFSET], edi
	mov [eax + PCBCTX_EBP_OFFSET], ebp

	; pushfd saves eflags to the top of the stack and pops to ecx
	pushfd
	pop ecx
	mov [eax + PCBCTX_EFLAGS_OFFSET], ecx

	; store the eip, after context siwtch back to this proc - execute from here
	; [esp] pointer to return address
	mov ecx, [esp]
	mov [eax + PCBCTX_EIP_OFFSET], ecx

	; store the esp offset after return
	mov ecx, [esp+4]
	mov [eax + PCBCTX_ESP_OFFSET], ecx

	; Load context from next->context
	; next pcb pointer
	mov eax, [esp+8]
	mov ebx, [eax + PCBCTX_EBX_OFFSET]
    mov ecx, [eax + PCBCTX_ECX_OFFSET]
    mov edx, [eax + PCBCTX_EDX_OFFSET]
    mov esi, [eax + PCBCTX_ESI_OFFSET]
    mov edi, [eax + PCBCTX_EDI_OFFSET]
    mov ebp, [eax + PCBCTX_EBP_OFFSET]
    
	mov ecx, [eax + PCBCTX_EFLAGS_OFFSET]
	push ecx
	popfd

	mov esp, [eax + PCBCTX_ESP_OFFSET]

	; jump to the next process
	mov ecx, [eax + PCBCTX_EIP_OFFSET]
	jmp ecx

