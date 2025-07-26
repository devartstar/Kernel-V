CC      := gcc
CFLAGS  := -m32 -ffreestanding -c -g -fno-pie -I kernel/include
NASM    := nasm
NASMFLAGS := -g -F dwarf

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

# Debug files with symbols
STAGE1_ELF = $(BUILDDIR)/stage1.elf
STAGE2_ELF = $(BUILDDIR)/stage2.elf

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
	$(NASM) $(NASMFLAGS) -f bin $< -o $@

# Build stage1 with ELF format for debugging symbols
$(STAGE1_ELF): $(STAGE1_SRC) | $(BUILDDIR)
	$(NASM) $(NASMFLAGS) -f elf32 $< -o $(BUILDDIR)/stage1.o
	ld -m elf_i386 -Ttext=0x7c00 --oformat=elf32-i386 $(BUILDDIR)/stage1.o -o $@

$(STAGE2_BIN): $(STAGE2_SRC) | $(BUILDDIR)
	$(NASM) $(NASMFLAGS) -f bin $< -o $@

# Build stage2 with ELF format for debugging symbols  
$(STAGE2_ELF): $(STAGE2_SRC) | $(BUILDDIR)
	$(NASM) $(NASMFLAGS) -f elf32 $< -o $(BUILDDIR)/stage2.o
	ld -m elf_i386 -Ttext=0x1000 --oformat=elf32-i386 $(BUILDDIR)/stage2.o -o $@

$(PRINTK_OBJ): $(PRINTK_SRC) $(PRINTK_HDR) | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@

$(VGA_OBJ): $(VGA_SRC) $(VGA_HDR) | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@

$(KERNEL_OBJ): $(KERNEL_MAIN_SRC) | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@

$(KERNEL_ENTRY_OBJ): $(KERNEL_ENTRY_SRC) | $(BUILDDIR)
	$(NASM) $(NASMFLAGS) -f elf32 $< -o $@

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

# === Debug targets ===
# Build all debug symbols
debug-symbols: $(STAGE1_ELF) $(STAGE2_ELF) $(KERNEL_ELF)

# Debug bootloader stage1 
debug-stage1: $(STAGE1_ELF) $(DISK_IMG)
	@echo "Starting QEMU with GDB server for Stage1 bootloader debugging..."
	@echo "Stage1 loads at 0x7c00. In GDB, use: target remote :1234"
	@echo "Set breakpoint with: (gdb) break *0x7c00"
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -s -S -display curses

# Debug bootloader stage2
debug-stage2: $(STAGE2_ELF) $(DISK_IMG)
	@echo "Starting QEMU with GDB server for Stage2 bootloader debugging..."
	@echo "Stage2 loads at 0x1000. In GDB, use: target remote :1234" 
	@echo "Set breakpoint with: (gdb) break *0x1000"
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -s -S -display curses

# Debug kernel
debug-kernel: $(KERNEL_ELF) $(DISK_IMG)
	@echo "Starting QEMU with GDB server for Kernel debugging..."
	@echo "Kernel loads at 0x10000. In GDB, use: target remote :1234"
	@echo "Set breakpoint with: (gdb) break kernel_main"
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -s -S -display curses

# === Debug target for GDB/QEMU ===
debug: $(KERNEL_ELF) $(DISK_IMG)
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -s -S -display curses

clean:
	rm -rf $(BUILDDIR)

# === GDB Helper Scripts ===
# Create GDB script for bootloader debugging
gdb-bootloader: debug-symbols
	@echo "Creating GDB script for bootloader debugging..."
	@echo "set architecture i8086" > $(BUILDDIR)/gdb_bootloader.txt
	@echo "target remote :1234" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "# Stage 1 (MBR) breakpoints" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "break *0x7c00" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "# Stage 2 breakpoints" >> $(BUILDDIR)/gdb_bootloader.txt  
	@echo "break *0x1000" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "# Kernel entry breakpoint" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "break *0x10000" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "continue" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "GDB script created: $(BUILDDIR)/gdb_bootloader.txt"
	@echo "Usage: gdb -x $(BUILDDIR)/gdb_bootloader.txt"

# Create GDB script for kernel debugging
gdb-kernel: debug-symbols
	@echo "Creating GDB script for kernel debugging..."
	@echo "set architecture i386" > $(BUILDDIR)/gdb_kernel.txt
	@echo "target remote :1234" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "symbol-file $(KERNEL_ELF)" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "break kernel_main" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "continue" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "GDB script created: $(BUILDDIR)/gdb_kernel.txt"
	@echo "Usage: gdb -x $(BUILDDIR)/gdb_kernel.txt"

.PHONY: all clean run debug debug-symbols debug-stage1 debug-stage2 debug-kernel gdb-bootloader gdb-kernel

