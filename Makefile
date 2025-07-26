CC      := gcc
CFLAGS  := -m32 -ffreestanding -c -g -fno-pie -I kernel/include
NASM    := nasm
NASMFLAGS := -g -F stabs

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

$(STAGE1_BIN): $(STAGE1_SRC) | $(BUILDDIR)
	$(NASM) -f bin $< -o $@

# Build stage1 with ELF format for debugging symbols
$(STAGE1_ELF): $(STAGE1_SRC) | $(BUILDDIR)
	$(NASM) -g -f elf32 -DELF_BUILD $< -o $(BUILDDIR)/stage1.o
	ld -m elf_i386 -Ttext=0x7c00 --oformat=elf32-i386 $(BUILDDIR)/stage1.o -o $@

$(STAGE2_BIN): $(STAGE2_SRC) | $(BUILDDIR)
	$(NASM) -f bin $< -o $@

# Build stage2 with ELF format for debugging symbols  
$(STAGE2_ELF): $(STAGE2_SRC) | $(BUILDDIR)
	$(NASM) -g -f elf32 -DELF_BUILD $< -o $(BUILDDIR)/stage2.o
	ld -m elf_i386 -Ttext=0x7e00 --oformat=elf32-i386 $(BUILDDIR)/stage2.o -o $@

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

# Verify debug symbols are built correctly
verify-symbols: debug-symbols
	@echo "Verifying debug symbols..."
	@echo "Stage1 ELF: $(STAGE1_ELF)"
	@ls -la $(STAGE1_ELF) 2>/dev/null || echo "Stage1 ELF not found!"
	@echo "Stage2 ELF: $(STAGE2_ELF)"  
	@ls -la $(STAGE2_ELF) 2>/dev/null || echo "Stage2 ELF not found!"
	@echo "Kernel ELF: $(KERNEL_ELF)"
	@ls -la $(KERNEL_ELF) 2>/dev/null || echo "Kernel ELF not found!"
	@echo "Checking for debug symbols in Stage1:"
	@objdump -h $(STAGE1_ELF) 2>/dev/null | grep debug || echo "No debug symbols in Stage1"
	@echo "Checking for debug symbols in Kernel:"
	@objdump -h $(KERNEL_ELF) 2>/dev/null | grep debug || echo "No debug symbols in Kernel"

# Debug bootloader stage1 
debug-stage1: $(STAGE1_ELF) $(DISK_IMG)
	@echo "Starting QEMU with GDB server for Stage1 bootloader debugging..."
	@echo "Stage1 loads at 0x7c00. In GDB, use: target remote :1234"
	@echo "Set breakpoint with: (gdb) break *0x7c00"
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -s -S -display curses

# Debug bootloader stage2
debug-stage2: $(STAGE2_ELF) $(DISK_IMG)
	@echo "Starting QEMU with GDB server for Stage2 bootloader debugging..."
	@echo "Stage2 loads at 0x7e00. In GDB, use: target remote :1234" 
	@echo "Set breakpoint with: (gdb) break *0x7e00"
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

# === Help target ===
help:
	@echo "Kernel-V Build and Debug Targets:"
	@echo ""
	@echo "Build targets:"
	@echo "  make           - Build complete OS image"
	@echo "  make clean     - Clean build directory"
	@echo "  make run       - Build and run in QEMU"
	@echo ""
	@echo "Debug targets:"
	@echo "  make debug-stage1    - Debug bootloader stage 1"
	@echo "  make debug-stage2    - Debug bootloader stage 2" 
	@echo "  make debug-kernel    - Debug kernel"
	@echo "  make debug-symbols   - Build all debug symbols"
	@echo "  make verify-symbols  - Check if debug symbols are built correctly"
	@echo ""
	@echo "GDB Helper targets:"
	@echo "  make gdb-bootloader       - Create GDB script for bootloader (with TUI)"
	@echo "  make gdb-kernel          - Create GDB script for kernel (with TUI)"
	@echo "  make gdb-bootloader-regs - Bootloader debug with registers layout"
	@echo "  make gdb-kernel-split    - Kernel debug with split layout"
	@echo ""
	@echo "Manual debugging steps:"
	@echo "  1. Terminal 1: make debug-stage1  (note: hyphen, not underscore)"
	@echo "  2. Terminal 2: gdb"
	@echo "  3. In GDB: target remote :1234"
	@echo "  4. In GDB: set architecture i8086"
	@echo "  5. In GDB: add-symbol-file build/stage1.elf 0x7c00"
	@echo "  6. In GDB: tui enable"
	@echo "  7. In GDB: layout asm"
	@echo "  8. In GDB: hbreak *0x7c00"
	@echo "  9. In GDB: continue"
	@echo ""
	@echo "Correct sequence for automated debugging:"
	@echo "  1. make clean && make"
	@echo "  2. Terminal 1: make debug-stage1"
	@echo "  3. Terminal 2: make gdb-bootloader"
	@echo "  4. Terminal 2: gdb -x build/gdb_bootloader.txt"
	@echo ""
	@echo "GDB TUI Layouts available:"
	@echo "  layout src    - Source + command"
	@echo "  layout asm    - Assembly + command"
	@echo "  layout split  - Source + assembly + command"
	@echo "  layout regs   - Registers + source/asm + command"
	@echo "  tui disable   - Exit TUI mode"
	@echo "  Ctrl+X+A      - Toggle TUI mode"
	@echo "  Ctrl+X+1      - Single window"
	@echo "  Ctrl+X+2      - Two windows"

# === GDB Helper Scripts ===
# Create GDB script for bootloader debugging
gdb-bootloader: debug-symbols
	@echo "Creating GDB script for bootloader debugging..."
	@echo "# Connect to QEMU" > $(BUILDDIR)/gdb_bootloader.txt
	@echo "target remote :1234" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "# Set 16-bit real mode architecture" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "set architecture i8086" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "# Load symbol files" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "add-symbol-file $(STAGE1_ELF) 0x7c00" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "add-symbol-file $(STAGE2_ELF) 0x7e00" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "# Enable TUI mode with layout" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "tui enable" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "layout asm" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "focus cmd" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "# Set breakpoints at key locations" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "hbreak *0x7c00" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "hbreak *0x7e00" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "hbreak *0x10000" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "# Display breakpoints and symbols" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "info breakpoints" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "info files" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "# Show current state" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "info registers" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "x/5i $$pc" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "GDB script created: $(BUILDDIR)/gdb_bootloader.txt"
	@echo "Usage: First run 'make debug-stage1' in one terminal"
	@echo "       Then run 'gdb -x $(BUILDDIR)/gdb_bootloader.txt' in another"

# Create GDB script for kernel debugging  
gdb-kernel: debug-symbols
	@echo "Creating GDB script for kernel debugging..."
	@echo "set architecture i386" > $(BUILDDIR)/gdb_kernel.txt
	@echo "target remote :1234" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "symbol-file $(KERNEL_ELF)" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "# Enable TUI mode with source layout" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "tui enable" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "layout src" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "focus cmd" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "break kernel_main" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "# Show breakpoints" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "info breakpoints" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "# Ready to debug - use 'continue' to start" >> $(BUILDDIR)/gdb_kernel.txt
	@echo "GDB script created: $(BUILDDIR)/gdb_kernel.txt"
	@echo "Usage: gdb -x $(BUILDDIR)/gdb_kernel.txt"

# Create advanced GDB script with register layout for bootloader
gdb-bootloader-regs: debug-symbols
	@echo "Creating advanced GDB script for bootloader debugging with registers..."
	@echo "# Connect to QEMU" > $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "target remote :1234" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "set architecture i8086" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "# Load symbol files" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "add-symbol-file $(STAGE1_ELF) 0x7c00" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "add-symbol-file $(STAGE2_ELF) 0x7e00" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "# Enable TUI with registers layout" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "tui enable" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "layout regs" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "focus cmd" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "hbreak *0x7c00" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "hbreak *0x7e00" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "hbreak *0x10000" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "info breakpoints" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "info files" >> $(BUILDDIR)/gdb_bootloader_regs.txt
	@echo "GDB script with registers created: $(BUILDDIR)/gdb_bootloader_regs.txt"
	@echo "Usage: gdb -x $(BUILDDIR)/gdb_bootloader_regs.txt"

# Create advanced GDB script with split layout for kernel  
gdb-kernel-split: debug-symbols
	@echo "Creating advanced GDB script for kernel debugging with split layout..."
	@echo "set architecture i386" > $(BUILDDIR)/gdb_kernel_split.txt
	@echo "target remote :1234" >> $(BUILDDIR)/gdb_kernel_split.txt
	@echo "symbol-file $(KERNEL_ELF)" >> $(BUILDDIR)/gdb_kernel_split.txt
	@echo "# Enable TUI with split layout (source + assembly)" >> $(BUILDDIR)/gdb_kernel_split.txt
	@echo "tui enable" >> $(BUILDDIR)/gdb_kernel_split.txt
	@echo "layout split" >> $(BUILDDIR)/gdb_kernel_split.txt
	@echo "focus cmd" >> $(BUILDDIR)/gdb_kernel_split.txt
	@echo "break kernel_main" >> $(BUILDDIR)/gdb_kernel_split.txt
	@echo "info breakpoints" >> $(BUILDDIR)/gdb_kernel_split.txt
	@echo "GDB script with split layout created: $(BUILDDIR)/gdb_kernel_split.txt"
	@echo "Usage: gdb -x $(BUILDDIR)/gdb_kernel_split.txt"

.PHONY: all clean run debug debug-symbols verify-symbols debug-stage1 debug-stage2 debug-kernel gdb-bootloader gdb-kernel gdb-bootloader-regs gdb-kernel-split help

