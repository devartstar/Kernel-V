; ------------------------------------------------------------------
; Minimal Multiboot2 header so QEMU (and any Multiboot2-capable loader)
; will map your ELF at 1 MiB and jump straight to _start
; ------------------------------------------------------------------

align 8
    dd 0xE85250D6               ; MBOOT2_MAGIC
    dd 0                        ; architecture (0 = i386)
    dd header_end - header_start ; total header size
    dd -(0xE85250D6 + 0 + (header_end - header_start))

header_start:
    dd 0                        ; tag type = END
    dd 8                        ; tag size = 8
header_end:

; ------------------------------------------------------------------
; Your real reset code begins here
; ------------------------------------------------------------------

BITS 32

global  _start                  ; make _start visible to the linker
extern  kernel_main             ; C entry point

_start:
    ; optional: set up a stack if your loader didn’t
    mov     esp, 0x9FB00

    call    kernel_main         ; jump into your C kernel

.hang:
    cli
    hlt
    jmp     .hang               ; spin here forever
