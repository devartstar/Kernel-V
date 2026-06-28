/*
 * user.ld - linker script for freestanding C user-space programs.
 *
 * Lays the program out as a flat image based at the user code virtual address
 * (0x00400000), with _start first so the kernel entry lands on it. The linked
 * ELF is converted to a raw binary (objcopy -O binary) and then embedded into
 * the test kernel, mirroring the NASM `-f bin` user blobs.
 */

ENTRY(_start)

SECTIONS
{
    . = 0x00400000;

    .text : {
        *(.text.start)
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(.bss*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
