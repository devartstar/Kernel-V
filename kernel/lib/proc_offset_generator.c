#include <stdio.h>
#include "proc_offset.h"

/**
* main - generate a header for assembly to consume
* the header proc_offset.h is not understandable by assembly because of use of
* offset, arithmetic on defines.
* A simple generator to create an header file with offset values
*/
int main ()
{
    printf ("; Auto-generated. DO NOT EDIT. \n");
    printf("%%define PCBCTX_EIP_OFFSET      %zu\n", (size_t)PCBCTX_EIP_OFFSET);
    printf("%%define PCBCTX_ESP_OFFSET      %zu\n", (size_t)PCBCTX_ESP_OFFSET);
    printf("%%define PCBCTX_EBP_OFFSET      %zu\n", (size_t)PCBCTX_EBP_OFFSET);
    printf("%%define PCBCTX_EAX_OFFSET      %zu\n", (size_t)PCBCTX_EAX_OFFSET);
    printf("%%define PCBCTX_EBX_OFFSET      %zu\n", (size_t)PCBCTX_EBX_OFFSET);
    printf("%%define PCBCTX_ECX_OFFSET      %zu\n", (size_t)PCBCTX_ECX_OFFSET);
    printf("%%define PCBCTX_EDX_OFFSET      %zu\n", (size_t)PCBCTX_EDX_OFFSET);
    printf("%%define PCBCTX_ESI_OFFSET      %zu\n", (size_t)PCBCTX_ESI_OFFSET);
    printf("%%define PCBCTX_EDI_OFFSET      %zu\n", (size_t)PCBCTX_EDI_OFFSET);
    printf("%%define PCBCTX_EFLAGS_OFFSET   %zu\n", (size_t)PCBCTX_EFLAGS_OFFSET);
    return 0;
}
