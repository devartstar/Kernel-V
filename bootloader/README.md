

### Stage 2 Bootloader

```
[Stage 2 bootloader in RAM]
     |
     |--- BIOS loads Stage 2 at e.g., 0x8000
     |--- Stage 2 loads kernel to 0x100000
     |
     |--- [enable protected mode]
     |--- jmp 0x08:protected_mode_start   <-- (far jump in new CS)
             |
             |--- Set up data segments, stack
             |--- jmp 0x100000           <-- (to kernel entry point)
                         |
                         |--- [Your kernel runs!]
```

### GDT (Global Descriptor Table)

Array of 8 byte entries
Entry 0 is null
Segment Selector - 16 bit selector [0-12 index bits] [TI 13 bit] [RPL 14-15 bit]
Index - which descriptor in the GDT you want
TI - 0 means use GDT, 1 means use LDT
RPL - Requested Privelege level (0 for kernel)

mov ax, 0x10 -> 16
table index = 2 - hence for every table stack entry use index 2
