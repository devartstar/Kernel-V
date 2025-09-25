global gdt_flush

gdt_flush:
    mov eax, [esp+4]  ; pointer to gdt_ptr struct passed as arg
    lgdt [eax]
    ret
