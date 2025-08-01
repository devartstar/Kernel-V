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
PANIK_SRC        = $(KERNDIR)/lib/panik.c
PANIK_HDR        = $(KERNDIR)/include/panik.h
TEST_PANIK_SRC   = $(KERNDIR)/tests/test_panik.c
TEST_PANIK_HDR   = $(KERNDIR)/include/tests/test_panik.h

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
PANIK_OBJ  = $(BUILDDIR)/panik.o
TEST_PANIK_OBJ = $(BUILDDIR)/test_panik.o
KERNEL_PANIK_OBJ = $(BUILDDIR)/test_panik.o
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

$(PANIK_OBJ): $(PANIK_SRC) $(PANIK_HDR) | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TEST_PANIK_OBJ): $(TEST_PANIK_SRC) $(TEST_PANIK_HDR) | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@

$(KERNEL_OBJ): $(KERNEL_MAIN_SRC) | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@

$(KERNEL_ENTRY_OBJ): $(KERNEL_ENTRY_SRC) | $(BUILDDIR)
	$(NASM) $(NASMFLAGS) -f elf32 $< -o $@

$(KERNEL_ELF): $(KERNEL_ENTRY_OBJ) $(PRINTK_OBJ) $(VGA_OBJ) $(PANIK_OBJ) $(TEST_PANIK_OBJ) $(KERNEL_OBJ) $(KERNEL_LD) | $(BUILDDIR)
	ld -m elf_i386 -T $(KERNEL_LD) -o $@ $(KERNEL_ENTRY_OBJ) $(PRINTK_OBJ) $(VGA_OBJ) $(PANIK_OBJ) $(TEST_PANIK_OBJ) $(KERNEL_OBJ) -nostdlib

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

# Debug both bootloader stages
debug-bootloader: $(STAGE1_ELF) $(STAGE2_ELF) $(DISK_IMG)
	@echo "Starting QEMU with GDB server for both Stage1 and Stage2 bootloader debugging..."
	@echo "Stage1 loads at 0x7c00, Stage2 loads at 0x7e00"
	@echo "In GDB, use: target remote :1234"
	@echo "Set breakpoints with: (gdb) break *0x7c00 and (gdb) break *0x7e00"
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
	@echo "  make debug-bootloader - Debug both bootloader stages (recommended)"
	@echo "  make debug-kernel    - Debug kernel"
	@echo "  make debug-symbols   - Build all debug symbols"
	@echo "  make verify-symbols  - Check if debug symbols are built correctly"
	@echo ""
	@echo "GDB Helper targets:"
	@echo "  make gdb-bootloader       - Create GDB script for bootloader (with TUI)"
	@echo "  make gdb-kernel          - Create GDB script for kernel (with TUI)"
	@echo "  make gdb-bootloader-regs - Bootloader debug with registers layout"
	@echo "  make gdb-kernel-split    - Kernel debug with split layout"
	@echo "  make gdb-full-debug      - Complete bootloader-to-kernel debugging"
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
	@echo "  2. Terminal 1: make debug-bootloader  (for both stages)"
	@echo "  3. Terminal 2: make gdb-bootloader"
	@echo "  4. Terminal 2: gdb -x build/gdb_bootloader.txt"
	@echo ""
	@echo "Alternative - debug individual stages:"
	@echo "  For Stage1 only: make debug-stage1"
	@echo "  For Stage2 only: make debug-stage2"
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
# Create GDB script for bootloader debugging (both stages)
gdb-bootloader: debug-symbols
	@echo "Creating GDB script for bootloader debugging (Stage1 + Stage2)..."
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
	@echo "# Debugging tips" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "echo" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "echo ==== Bootloader Debug Session ====" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "echo Stage1 starts at 0x7c00" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "echo Stage2 starts at 0x7e00" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "echo Kernel starts at 0x10000" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "echo Use 'continue' to run to first breakpoint" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "echo Use 'stepi' to step one instruction" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "echo Use 'info registers' to see register state" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "echo ====================================" >> $(BUILDDIR)/gdb_bootloader.txt
	@echo "GDB script created: $(BUILDDIR)/gdb_bootloader.txt"
	@echo "Usage: First run 'make debug-bootloader' in one terminal"
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

# Create GDB script for seamless bootloader-to-kernel debugging
gdb-full-debug: debug-symbols
	@echo "Creating comprehensive GDB script for bootloader-to-kernel debugging..."
	@echo "# === BOOTLOADER PHASE ===" > $(BUILDDIR)/gdb_full_debug.txt
	@echo "target remote :1234" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "set architecture i8086" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "add-symbol-file $(STAGE1_ELF) 0x7c00" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "add-symbol-file $(STAGE2_ELF) 0x7e00" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "tui enable" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "layout asm" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "focus cmd" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "hbreak *0x7c00" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "hbreak *0x7e00" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "# === KERNEL TRANSITION POINT ===" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "define switch-to-kernel" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  set architecture i386" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  add-symbol-file $(KERNEL_ELF) 0x10000" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  hbreak *0x10000" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  break kernel_main" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  layout split" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo ==== SWITCHED TO KERNEL DEBUGGING ====" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo Now debugging in 32-bit protected mode" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo Breakpoints set at 0x10000 and kernel_main" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo Use 'continue' to proceed to kernel entry" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo =========================================" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  info breakpoints" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "end" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "# === KERNEL LOADING VERIFICATION ===" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "define check-kernel-loaded" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo Checking if kernel was loaded at 0x10000..." >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  x/10i 0x10000" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo First 32 bytes of kernel memory:" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  x/32b 0x10000" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo If you see all zeros or repeated 0x00 0x00, kernel didn't load!" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "end" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "# === DISK LOADING DEBUG ===" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "define debug-disk-load" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo Setting breakpoint at disk loading section..." >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  # Set breakpoint right after disk read" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  # You'll need to find the exact address in stage2" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo After disk read, check:" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo   x/10b 0x10000  - to see if kernel loaded" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "  echo   info registers - to check carry flag for errors" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "end" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "# === INITIAL SETUP ===" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo ==== BOOTLOADER-TO-KERNEL DEBUG SESSION ====" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo Stage1: 0x7c00, Stage2: 0x7e00, Kernel: 0x10000" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo Available commands:" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo   check-kernel-loaded  - Verify if kernel loaded at 0x10000" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo   debug-disk-load      - Debug disk loading process" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo   switch-to-kernel     - Switch to kernel debugging mode" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo When you reach the kernel jump point:" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo   1. Type: check-kernel-loaded" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo   2. If kernel loaded: switch-to-kernel" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo   3. Then: continue" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "echo =============================================" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "info breakpoints" >> $(BUILDDIR)/gdb_full_debug.txt
	@echo "GDB comprehensive debug script created: $(BUILDDIR)/gdb_full_debug.txt"
	@echo "Usage: gdb -x $(BUILDDIR)/gdb_full_debug.txt"

.PHONY: all clean run debug debug-symbols verify-symbols debug-stage1 debug-stage2 debug-bootloader debug-kernel gdb-bootloader gdb-kernel gdb-bootloader-regs gdb-kernel-split gdb-full-debug help

