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

STAGE1_OBJ = $(BUILDDIR)/stage1.o
STAGE2_OBJ = $(BUILDDIR)/stage2.o

STAGE1_ELF = $(BUILDDIR)/stage1.elf
STAGE2_ELF = $(BUILDDIR)/stage2.elf

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

# --- Stage 1: binary and ELF ---
$(STAGE1_BIN): $(STAGE1_SRC) | $(BUILDDIR)
	nasm -DBIN -f bin $< -o $@

$(STAGE1_OBJ): $(STAGE1_SRC) | $(BUILDDIR)
	nasm -f elf32 -g $< -o $@

$(STAGE1_ELF): $(STAGE1_OBJ) | $(BUILDDIR)
	ld -m elf_i386 -Ttext 0x7c00 --oformat=elf32-i386 $< -o $@

# --- Stage 2: binary and ELF ---
$(STAGE2_BIN): $(STAGE2_SRC) | $(BUILDDIR)
	nasm -f bin $< -o $@

$(STAGE2_OBJ): $(STAGE2_SRC) | $(BUILDDIR)
	nasm -f elf32 -g $< -o $@

$(STAGE2_ELF): $(STAGE2_OBJ) | $(BUILDDIR)
	ld -m elf_i386 -Ttext 0x8000 --oformat=elf32-i386 $< -o $@

# --- Kernel: ELF and BIN ---
$(KERNEL_ENTRY_OBJ): $(KERNEL_ENTRY_SRC) | $(BUILDDIR)
	nasm -f elf32 $< -o $@

$(KERNEL_OBJ): $(KERNEL_MAIN_SRC) | $(BUILDDIR)
	gcc -m32 -ffreestanding -g -c $< -o $@

$(KERNEL_ELF): $(KERNEL_ENTRY_OBJ) $(KERNEL_OBJ) $(KERNEL_LD) | $(BUILDDIR)
	ld -m elf_i386 -T $(KERNEL_LD) -o $@ $(KERNEL_ENTRY_OBJ) $(KERNEL_OBJ)

$(KERNEL_BIN): $(KERNEL_ELF) | $(BUILDDIR)
	objcopy -O binary $< $@

# --- Disk image assembly ---
$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) | $(BUILDDIR)
	dd if=/dev/zero of=$@ bs=1K count=1440
	dd if=$(STAGE1_BIN) of=$@ bs=512 seek=0 conv=notrunc
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc
	dd if=$(KERNEL_BIN)  of=$@ bs=512 seek=$(KERNEL_LBA) conv=notrunc

run: $(DISK_IMG)
	qemu-system-i386 -curses -drive format=raw,file=$(DISK_IMG)

debug: $(DISK_IMG) $(STAGE1_ELF) $(STAGE2_ELF) $(KERNEL_ELF)
	@echo "In one GDB session, you can load all symbols like this:"
	@echo ""
	@echo "  gdb"
	@echo "  (gdb) symbol-file $(STAGE1_ELF)"
	@echo "  (gdb) add-symbol-file $(STAGE2_ELF) 0x6000"
	@echo "  (gdb) add-symbol-file $(KERNEL_ELF)  0x100000"
	@echo "  (gdb) target remote localhost:1234"
	@echo ""
	qemu-system-i386 -s -S -curses -drive format=raw,file=$(DISK_IMG)

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean run debug
