CC      := gcc
CFLAGS  := -m32 -ffreestanding -c -g -fno-pie -I kernel/include
NASM    := nasm

BOOTDIR   = bootloader
KERNDIR   = kernel
BUILDDIR  = build

STAGE1_SRC = $(BOOTDIR)/stage1.asm
STAGE2_SRC = $(BOOTDIR)/stage2.asm

KERNEL_ENTRY_SRC = $(KERNDIR)/arch/x86/kernel_entry.asm
KERNEL_MAIN_SRC  = $(KERNDIR)/main/kernel.c
KERNEL_LD        = $(KERNDIR)/linker/kernel.ld
PRINTK_SRC       = $(KERNDIR)/lib/printk.c
PRINTK_HDR       = $(KERNDIR)/include/printk.h
VGA_SRC		 = $(KERNDIR)/drivers/vga/vga.c
VGA_HDR		 = $(KERNDIR)/include/drivers/vga.h

STAGE1_BIN = $(BUILDDIR)/stage1.bin
STAGE2_BIN = $(BUILDDIR)/stage2.bin
KERNEL_BIN = $(BUILDDIR)/kernel.bin
KERNEL_ELF = $(BUILDDIR)/kernel.elf
DISK_IMG   = $(BUILDDIR)/disk.img

# --- Kernel objects ---
PRINTK_OBJ = $(BUILDDIR)/printk.o
KERNEL_OBJ = $(BUILDDIR)/kernel.o
VGA_OBJ	   = $(BUILDDIR)/vga.o
KERNEL_ENTRY_OBJ = $(BUILDDIR)/kernel_entry.o

all: $(DISK_IMG)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)
	$(BOOTDIR)/image.sh

$(STAGE1_BIN): $(STAGE1_SRC) | $(BUILDDIR)
	$(NASM) -DBIN -f bin $< -o $@

$(STAGE2_BIN): $(STAGE2_SRC) | $(BUILDDIR)
	$(NASM) -DBIN -f bin $< -o $@

$(PRINTK_OBJ): $(PRINTK_SRC) $(PRINTK_HDR) | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@

$(VGA_OBJ): $(VGA_SRC) $(VGA_HDR) | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@

$(KERNEL_OBJ): $(KERNEL_MAIN_SRC) | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@

$(KERNEL_ENTRY_OBJ): $(KERNEL_ENTRY_SRC) | $(BUILDDIR)
	$(NASM) -f elf32 -g $< -o $@

$(KERNEL_ELF): $(KERNEL_ENTRY_OBJ) $(PRINTK_OBJ) $(VGA_OBJ) $(KERNEL_OBJ) $(KERNEL_LD) | $(BUILDDIR)
	ld -m elf_i386 -T $(KERNEL_LD) -o $@ $(KERNEL_ENTRY_OBJ) $(PRINTK_OBJ) $(VGA_OBJ) $(KERNEL_OBJ) -nostdlib

$(KERNEL_BIN): $(KERNEL_ELF) | $(BUILDDIR)
	objcopy -O binary $< $@

$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) | $(BUILDDIR)
	dd if=/dev/zero of=$@ bs=1K count=1440
	dd if=$(STAGE1_BIN) of=$@ bs=512 seek=0 conv=notrunc
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=9 conv=notrunc

run: $(DISK_IMG)
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -display curses

# === Debug target for GDB/QEMU ===
debug: $(KERNEL_ELF) $(DISK_IMG)
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -s -S -display curses

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean run debug

