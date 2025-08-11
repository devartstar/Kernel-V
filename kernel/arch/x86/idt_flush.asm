[BITS 32]
[GLOBAL idt_flush]

idt_flush:
    mov eax, [esp + 4]
    lidtr [eax]
    ret