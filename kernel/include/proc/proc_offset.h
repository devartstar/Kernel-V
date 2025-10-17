#ifndef PROC_OFFSETS_H
#define PROC_OFFSETS_H

#include <stddef.h>
#include "proc.h"

#define PCB_CONTEXT_OFFSET		offsetof(pcb_t, context)

#define CTX_EIP_OFFSET			offsetof(regs_context_t, eip)
#define CTX_ESP_OFFSET			offsetof(regs_context_t, esp)
#define CTX_EBP_OFFSET			offsetof(regs_context_t, ebp)
#define CTX_EAX_OFFSET			offsetof(regs_context_t, eax)
#define CTX_EBX_OFFSET			offsetof(regs_context_t, ebx)
#define CTX_ECX_OFFSET			offsetof(regs_context_t, ecx)
#define CTX_EDX_OFFSET			offsetof(regs_context_t, edx)
#define CTX_ESI_OFFSET			offsetof(regs_context_t, esi)
#define CTX_EDI_OFFSET			offsetof(regs_context_t, edi)
#define CTX_EFLAGS_OFFSET		offsetof(regs_context_t, eflags)

#define PCBCTX_EIP_OFFSET      (PCB_CONTEXT_OFFSET + CTX_EIP_OFFSET)
#define PCBCTX_ESP_OFFSET      (PCB_CONTEXT_OFFSET + CTX_ESP_OFFSET)
#define PCBCTX_EBP_OFFSET      (PCB_CONTEXT_OFFSET + CTX_EBP_OFFSET)
#define PCBCTX_EAX_OFFSET      (PCB_CONTEXT_OFFSET + CTX_EAX_OFFSET)
#define PCBCTX_EBX_OFFSET      (PCB_CONTEXT_OFFSET + CTX_EBX_OFFSET)
#define PCBCTX_ECX_OFFSET      (PCB_CONTEXT_OFFSET + CTX_ECX_OFFSET)
#define PCBCTX_EDX_OFFSET      (PCB_CONTEXT_OFFSET + CTX_EDX_OFFSET)
#define PCBCTX_ESI_OFFSET      (PCB_CONTEXT_OFFSET + CTX_ESI_OFFSET)
#define PCBCTX_EDI_OFFSET      (PCB_CONTEXT_OFFSET + CTX_EDI_OFFSET)
#define PCBCTX_EFLAGS_OFFSET   (PCB_CONTEXT_OFFSET + CTX_EFLAGS_OFFSET)

#endif
