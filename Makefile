# --- Toolchain ---
CC      := gcc
CFLAGS  := -m32 -ffreestanding -c -g -fno-pie -I kernel/include
NASM    := nasm
NASMFLAGS := -g -F stabs

# --- Directories ---
BOOTDIR   = bootloader
KERNDIR   = kernel
BUILDDIR  = build

# --- Source Files ---
STAGE1_SRC 			= $(BOOTDIR)/stage1.asm
STAGE2_SRC 			= $(BOOTDIR)/stage2.asm
KERNEL_ENTRY_SRC 	= $(KERNDIR)/arch/x86/kernel_entry.asm
KERNEL_MAIN_SRC  	= $(KERNDIR)/main/kernel.c
KERNEL_LD        	= $(KERNDIR)/linker/kernel.ld

VGA_SRC          	= $(KERNDIR)/drivers/vga/vga.c

PRINTK_SRC       	= $(KERNDIR)/lib/printk.c
PANIK_SRC        	= $(KERNDIR)/lib/panik.c
TEST_PANIK_SRC   	= $(KERNDIR)/tests/test_panik.c
TEST_PRINTK_SRC  	= $(KERNDIR)/tests/test_printk.c

MEMORY_MAP_SRC   	= $(KERNDIR)/memory/memory_map.c
MEMORY_MNG_SRC   	= $(KERNDIR)/memory/pmm.c
MEMORY_PAGING_SRC 	= $(KERNDIR)/memory/paging.c
MEMORY_PAGE_FAULT_SRC = $(KERNDIR)/memory/page_fault.c
MEMORY_POOL_SRC  	= $(KERNDIR)/lib/pool_alloc.c

PROC_SRC		  	= $(KERNDIR)/proc/proc.c

IDT_SRC          	= $(KERNDIR)/arch/x86/idt.c
TSS_SRC             = $(KERNDIR)/arch/x86/tss.c
GDT_SRC             = $(KERNDIR)/arch/x86/gdt.c
GDT_FLUSH_SRC       = $(KERNDIR)/arch/x86/gdt_flush.asm
DOUBLE_FAULT_SRC    = $(KERNDIR)/arch/x86/double_fault_handler.asm

# --- Header Files ---
PRINTK_HDR       	= $(KERNDIR)/include/printk.h
VGA_HDR          	= $(KERNDIR)/include/drivers/vga.h
PANIK_HDR        	= $(KERNDIR)/include/panik.h

MEMORY_MAP_HDR   	= $(KERNDIR)/include/memory_map.h
MEMORY_MNG_HDR	 	= $(KERNDIR)/include/memory/pmm.h
MEMORY_PAGING_HDR 	= $(KERNDIR)/include/memory/paging.h
MEMORY_POOL_HDR 	= $(KERNDIR)/include/pool_alloc.h

PROC_HDR		  	= $(KERNDIR)/include/proc.h

TEST_PANIK_HDR   	= $(KERNDIR)/include/tests/test_panik.h
TEST_PRINTK_HDR  	= $(KERNDIR)/include/tests/test_printk.h

IDT_HDR		  		= $(KERNDIR)/include/idt.h
TSS_HDR             = $(KERNDIR)/include/arch/x86/tss.h
GDT_HDR             = $(KERNDIR)/include/arch/x86/gdt.h

# --- Output Files ---
STAGE1_BIN 			= $(BUILDDIR)/stage1.bin
STAGE2_BIN 			= $(BUILDDIR)/stage2.bin

# Debug files with symbols
STAGE1_ELF 			= $(BUILDDIR)/stage1.elf
STAGE2_ELF 			= $(BUILDDIR)/stage2.elf

# --- Object Files ---
PRINTK_OBJ      	= $(BUILDDIR)/printk.o
KERNEL_OBJ      	= $(BUILDDIR)/kernel.o
VGA_OBJ         	= $(BUILDDIR)/vga.o
PANIK_OBJ       	= $(BUILDDIR)/panik.o
TEST_PANIK_OBJ  	= $(BUILDDIR)/test_panik.o
KERNEL_ENTRY_OBJ	= $(BUILDDIR)/kernel_entry.o
TEST_PRINTK_OBJ 	= $(BUILDDIR)/test_printk.o

MEMORY_MAP_OBJ  	= $(BUILDDIR)/memory_map.o
MEMORY_MNG_OBJ  	= $(BUILDDIR)/pmm.o
MEMORY_PAGING_OBJ	= $(BUILDDIR)/paging.o
MEMORY_PAGE_FAULT_OBJ = $(BUILDDIR)/page_fault.o
MEMORY_POOL_OBJ  	= $(BUILDDIR)/pool_alloc.o

PROC_OBJ			= $(BUILDDIR)/proc.o

IDT_OBJ				= $(BUILDDIR)/idt.o
IDT_FLUSH_OBJ      = $(BUILDDIR)/idt_flush.o
ISR_PAGE_FAULT_OBJ = $(BUILDDIR)/isr_page_fault.o
TSS_OBJ            = $(BUILDDIR)/tss.o
GDT_OBJ            = $(BUILDDIR)/gdt.o
GDT_FLUSH_OBJ      = $(BUILDDIR)/gdt_flush.o
DOUBLE_FAULT_OBJ   = $(BUILDDIR)/double_fault_handler.o

# --- Object Groups ---
KERNEL_OBJS = $(KERNEL_ENTRY_OBJ) $(PRINTK_OBJ) $(VGA_OBJ) $(PANIK_OBJ) $(TEST_PANIK_OBJ) $(MEMORY_MAP_OBJ) $(MEMORY_MNG_OBJ) $(MEMORY_PAGING_OBJ) $(MEMORY_PAGE_FAULT_OBJ) $(IDT_OBJ) $(IDT_FLUSH_OBJ) $(ISR_PAGE_FAULT_OBJ) $(TSS_OBJ) $(GDT_OBJ) $(GDT_FLUSH_OBJ) $(DOUBLE_FAULT_OBJ) $(MEMORY_POOL_OBJ) $(PROC_OBJ) $(KERNEL_OBJ)
KERNEL_TEST_OBJS = $(KERNEL_OBJS) $(TEST_PRINTK_OBJ)

# --- Kernel ELF/BIN for test and non-test ---
KERNEL_ELF        = $(BUILDDIR)/kernel.elf
KERNEL_BIN        = $(BUILDDIR)/kernel.bin
KERNEL_TEST_ELF   = $(BUILDDIR)/kernel_test.elf
KERNEL_TEST_BIN   = $(BUILDDIR)/kernel_test.bin

# --- Disk images ---
DISK_IMG      = $(BUILDDIR)/disk.img
DISK_TEST_IMG = $(BUILDDIR)/disk_test.img

# --- Default target ---
all: $(DISK_IMG)

# --- Directory creation ---
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# --- Bootloader ---
$(STAGE1_BIN): $(STAGE1_SRC) | $(BUILDDIR)
	$(NASM) -f bin $< -o $@

$(STAGE2_BIN): $(STAGE2_SRC) | $(BUILDDIR)
	$(NASM) -f bin $< -o $@

# --- Bootloader ELF for debugging ---
$(STAGE1_ELF): $(STAGE1_SRC) | $(BUILDDIR)
	$(NASM) -g -f elf32 -DELF_BUILD $< -o $(BUILDDIR)/stage1.o
	ld -m elf_i386 -Ttext=0x7c00 --oformat=elf32-i386 $(BUILDDIR)/stage1.o -o $@

$(STAGE2_ELF): $(STAGE2_SRC) | $(BUILDDIR)
	$(NASM) -g -f elf32 -DELF_BUILD $< -o $(BUILDDIR)/stage2.o
	ld -m elf_i386 -Ttext=0x7e00 --oformat=elf32-i386 $(BUILDDIR)/stage2.o -o $@

# --- Pattern rules for C objects ---
$(BUILDDIR)/%.o: $(KERNDIR)/lib/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@
$(BUILDDIR)/%.o: $(KERNDIR)/drivers/vga/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@
$(BUILDDIR)/%.o: $(KERNDIR)/main/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@
$(BUILDDIR)/%.o: $(KERNDIR)/memory/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@
$(BUILDDIR)/%.o: $(KERNDIR)/proc/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@
$(BUILDDIR)/%.o: $(KERNDIR)/tests/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@
$(BUILDDIR)/%.o: $(KERNDIR)/arch/x86/%.asm | $(BUILDDIR)
	$(NASM) $(NASMFLAGS) -f elf32 $< -o $@
$(BUILDDIR)/%.o: $(KERNDIR)/arch/x86/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@

# --- Kernel ELF/BIN (non-test) ---
$(KERNEL_ELF): $(KERNEL_OBJS) $(KERNEL_LD) | $(BUILDDIR)
	ld -m elf_i386 -T $(KERNEL_LD) -o $@ $(KERNEL_OBJS) -nostdlib

$(KERNEL_BIN): $(KERNEL_ELF) | $(BUILDDIR)
	objcopy -O binary $< $@

# --- Kernel ELF/BIN (test build) ---
$(KERNEL_TEST_ELF): $(KERNEL_TEST_OBJS) $(KERNEL_LD) | $(BUILDDIR)
	ld -m elf_i386 -T $(KERNEL_LD) -o $@ $(KERNEL_TEST_OBJS) -nostdlib

$(KERNEL_TEST_BIN): $(KERNEL_TEST_ELF) | $(BUILDDIR)
	objcopy -O binary $< $@

# --- Disk images ---
$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) | $(BUILDDIR)
	dd if=/dev/zero of=$@ bs=1K count=1440
	dd if=$(STAGE1_BIN) of=$@ bs=512 seek=0 conv=notrunc
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=9 conv=notrunc

$(DISK_TEST_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_TEST_BIN) | $(BUILDDIR)
	dd if=/dev/zero of=$@ bs=1K count=1440
	dd if=$(STAGE1_BIN) of=$@ bs=512 seek=0 conv=notrunc
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc
	dd if=$(KERNEL_TEST_BIN) of=$@ bs=512 seek=9 conv=notrunc

# --- Run targets ---
run: $(DISK_IMG)
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -display curses

test: CFLAGS += -DKERNEL_TESTS
test: $(DISK_TEST_IMG)
	qemu-system-i386 -drive format=raw,file=$(DISK_TEST_IMG) -display curses

# --- Debug targets ---
debug-symbols: $(STAGE1_ELF) $(STAGE2_ELF) $(KERNEL_ELF)

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

debug-stage1: $(STAGE1_ELF) $(DISK_IMG)
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -s -S -display curses

debug-stage2: $(STAGE2_ELF) $(DISK_IMG)
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -s -S -display curses

debug-kernel: $(KERNEL_ELF) $(DISK_IMG)
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -s -S -display curses

debug-bootloader: $(STAGE1_ELF) $(STAGE2_ELF) $(DISK_IMG)
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -s -S -display curses

debug: $(KERNEL_ELF) $(DISK_IMG)
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -s -S -display curses

# --- Clean ---
clean:
	rm -rf $(BUILDDIR)

# --- Help ---
help:
	@echo "Kernel-V Build and Debug Targets:"
	@echo ""
	@echo "Build targets:"
	@echo "  make           - Build complete OS image"
	@echo "  make clean     - Clean build directory"
	@echo "  make run       - Build and run in QEMU"
	@echo "  make test      - Build and run kernel with tests enabled"
	@echo ""
	@echo "Debug targets:"
	@echo "  make debug-stage1    - Debug bootloader stage 1"
	@echo "  make debug-stage2    - Debug bootloader stage 2"
	@echo "  make debug-bootloader - Debug both bootloader stages"
	@echo "  make debug-kernel    - Debug kernel"
	@echo "  make debug-symbols   - Build all debug symbols"
	@echo "  make verify-symbols  - Check if debug symbols are built correctly"
	@echo ""
	@echo "GDB Helper targets:"
	@echo "  make gdb-bootloader       - Create GDB script for bootloader (with TUI)"
	@echo "  make gdb-kernel           - Create GDB script for kernel (with TUI)"
	@echo "  make gdb-bootloader-regs  - Bootloader debug with registers layout"
	@echo "  make gdb-kernel-split     - Kernel debug with split layout"
	@echo "  make gdb-full-debug       - Complete bootloader-to-kernel debugging"
	@echo ""
	@echo "Manual debugging steps:"
	@echo "  1. Terminal 1: make debug-stage1"
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
	@echo "  2. Terminal 1: make debug-bootloader"
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

# --- GDB Helper Scripts ---
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

.PHONY: all clean run test debug debug-symbols verify-symbols debug-stage1 debug-stage2 debug-bootloader debug-kernel help
