# ==== Directories ====
BOOTDIR   = bootloader
KERNDIR   = kernel
BUILDDIR  = build

# ==== Sources ====
STAGE1_SRC = $(BOOTDIR)/stage1.asm
STAGE2_SRC = $(BOOTDIR)/stage2.asm

KERNEL_ENTRY_SRC = $(KERNDIR)/arch/x86/kernel_entry.asm
KERNEL_MAIN_SRC  = $(KERNDIR)/main/kernel.c
KERNEL_LD        = $(KERNDIR)/linker/kernel.ld

# ==== Outputs ====
STAGE1_BIN = $(BUILDDIR)/stage1.bin
STAGE2_BIN = $(BUILDDIR)/stage2.bin

KERNEL_ENTRY_OBJ = $(BUILDDIR)/kernel_entry.o
KERNEL_OBJ       = $(BUILDDIR)/kernel.o

KERNEL_BIN = $(BUILDDIR)/kernel.bin
KERNEL_ELF = $(BUILDDIR)/kernel.elf

DISK_IMG   = $(BUILDDIR)/disk.img

# ==== Bootloader config ====
STAGE2_SECTORS := 8
KERNEL_LBA := 9

# ==== Build Rules ====
all: $(DISK_IMG)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(STAGE1_BIN): $(STAGE1_SRC) | $(BUILDDIR)
	nasm -f bin $< -o $@

$(STAGE2_BIN): $(STAGE2_SRC) | $(BUILDDIR)
	nasm -f bin $< -o $@

$(KERNEL_ENTRY_OBJ): $(KERNEL_ENTRY_SRC) | $(BUILDDIR)
	nasm -f elf32 $< -o $@

$(KERNEL_OBJ): $(KERNEL_MAIN_SRC) | $(BUILDDIR)
	gcc -m32 -ffreestanding -g -c $< -o $@

# -- Link kernel as ELF (with debug info)
$(KERNEL_ELF): $(KERNEL_ENTRY_OBJ) $(KERNEL_OBJ) $(KERNEL_LD) | $(BUILDDIR)
	ld -m elf_i386 -T $(KERNEL_LD) -o $@ $(KERNEL_ENTRY_OBJ) $(KERNEL_OBJ)

# -- Create raw binary for booting
$(KERNEL_BIN): $(KERNEL_ELF) | $(BUILDDIR)
	objcopy -O binary $< $@

# -- Build disk image with all components
$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) | $(BUILDDIR)
	dd if=/dev/zero of=$@ bs=1K count=1440
	dd if=$(STAGE1_BIN) of=$@ bs=512 seek=0 conv=notrunc
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc
	dd if=$(KERNEL_BIN)  of=$@ bs=512 seek=$(KERNEL_LBA) conv=notrunc

run: $(DISK_IMG)
	qemu-system-i386 -curses -drive format=raw,file=$(DISK_IMG)

debug: $(DISK_IMG) $(KERNEL_ELF)
	@echo "Run this in another terminal:"
	@echo "  gdb $(KERNEL_ELF)"
	@echo "Then in GDB:"
	@echo "  (gdb) target remote localhost:1234"
	@echo ""
	qemu-system-i386 -s -S -curses -drive format=raw,file=$(DISK_IMG)

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean run debug
