
# ===== Paths =====
BOOTDIR   = bootloader
KERNDIR   = kernel
BUILDDIR  = build

# ===== Files =====
STAGE1_SRC = $(BOOTDIR)/stage1.asm
STAGE2_SRC = $(BOOTDIR)/stage2.asm

KERNEL_ENTRY_SRC = $(KERNDIR)/arch/x86/kernel_entry.asm
KERNEL_MAIN_SRC  = $(KERNDIR)/main/kernel.c
KERNEL_LD        = $(KERNDIR)/linker/kernel.ld

STAGE1_BIN = $(BUILDDIR)/stage1.bin
STAGE2_BIN = $(BUILDDIR)/stage2.bin
KERNEL_BIN = $(BUILDDIR)/kernel.bin
DISK_IMG   = $(BUILDDIR)/disk.img

# ===== Targets =====
all: $(DISK_IMG)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(STAGE1_BIN): $(STAGE1_SRC) | $(BUILDDIR)
	nasm -DBIN -f bin $< -o $@

$(STAGE2_BIN): $(STAGE2_SRC) | $(BUILDDIR)
	nasm -DBIN -f bin $< -o $@

$(BUILDDIR)/kernel_entry.o: $(KERNEL_ENTRY_SRC) | $(BUILDDIR)
	nasm -f elf32 $< -o $@

$(BUILDDIR)/kernel.o: $(KERNEL_MAIN_SRC) | $(BUILDDIR)
	gcc -m32 -ffreestanding -c $< -o $@ -fno-pie

$(KERNEL_BIN): $(BUILDDIR)/kernel_entry.o $(BUILDDIR)/kernel.o $(KERNEL_LD) | $(BUILDDIR)
	ld -m elf_i386 -T $(KERNEL_LD) -o $@ $(BUILDDIR)/kernel_entry.o $(BUILDDIR)/kernel.o

$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) | $(BUILDDIR)
	dd if=/dev/zero of=$@ bs=1K count=1440
	dd if=$(STAGE1_BIN) of=$@ bs=512 seek=0 conv=notrunc
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=9 conv=notrunc

run: $(DISK_IMG)
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -display curses

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean run
