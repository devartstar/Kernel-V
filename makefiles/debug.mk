# ==============================================================================
# Debug Build Configuration & Advanced Debugging Features
# ==============================================================================

# Override settings for debug builds  
ENABLE_DEBUG := 1

# Debug-specific disk images
DISK_DEBUG_IMG := $(BUILDDIR)/disk_debug.img

.PHONY: debug-symbols verify-symbols debug-stage1 debug-stage2 debug-bootloader debug-kernel
.PHONY: gdb-bootloader gdb-kernel gdb-bootloader-regs gdb-kernel-split gdb-full-debug
.PHONY: connect-gdb debug-kernel-auto gdb-kernel-auto

# --- Debug Symbols ---
debug-symbols: bootloader kernel ## Build all debug symbols

verify-symbols: debug-symbols ## Verify debug symbols are present
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

# --- Individual Debug Targets ---
debug-stage1: debug-symbols all ## Debug Stage 1 bootloader
	$(ECHO) "Starting QEMU for Stage1 debugging..."
	$(ECHO) "Connect with: $(GDB) -x tools/gdb/stage1.gdb"
	$(Q)$(QEMU) -drive format=raw,file=$(DISK_IMG) -s -S -display curses

debug-stage2: debug-symbols all ## Debug Stage 2 bootloader
	$(ECHO) "Starting QEMU for Stage2 debugging..."
	$(ECHO) "Connect with: $(GDB) -x tools/gdb/stage2.gdb"
	$(Q)$(QEMU) -drive format=raw,file=$(DISK_IMG) -s -S -display curses

debug-bootloader: debug-symbols all ## Debug both bootloader stages
	$(ECHO) "Starting QEMU for bootloader debugging..."
	$(ECHO) "Connect with: $(GDB) -x tools/gdb/bootloader.gdb"
	$(Q)$(QEMU) -drive format=raw,file=$(DISK_IMG) -s -S -display curses

debug-kernel: debug-symbols all ## Debug kernel only
	$(ECHO) "Starting QEMU for kernel debugging..."  
	$(ECHO) "Connect with: $(GDB) -x tools/gdb/kernel.gdb"
	$(Q)$(QEMU) -drive format=raw,file=$(DISK_IMG) -s -S -display curses

# --- Advanced GDB Script Generation ---
connect-gdb: ## Connect GDB to running QEMU (use existing kernel.gdb)
	@echo "Connecting GDB to running QEMU session..."
	@echo "Make sure QEMU is running in another terminal!"
	$(GDB) -x tools/gdb/kernel.gdb

debug-kernel-auto: debug-symbols all gdb-kernel-auto ## Start QEMU and auto-connect GDB
	@echo "Starting QEMU in background..."
	@$(QEMU) -drive format=raw,file=$(DISK_IMG) -s -S -display curses &
	@echo "Waiting for QEMU to start..."
	@sleep 2
	@echo "Connecting GDB..."
	@$(GDB) -x tools/gdb/kernel_auto.gdb

# --- Advanced GDB Script Generation ---
gdb-kernel-auto: debug-symbols ## Generate auto-navigating kernel GDB script
	@echo "Creating auto-navigating GDB script for kernel debugging..."
	@mkdir -p tools/gdb
	@echo "set architecture i386" > tools/gdb/kernel_auto.gdb
	@echo "target remote :1234" >> tools/gdb/kernel_auto.gdb
	@echo "symbol-file $(KERNEL_ELF)" >> tools/gdb/kernel_auto.gdb
	@echo "add-symbol-file $(STAGE1_ELF) 0x7c00" >> tools/gdb/kernel_auto.gdb
	@echo "add-symbol-file $(STAGE2_ELF) 0x7e00" >> tools/gdb/kernel_auto.gdb
	@echo "# Set source directories" >> tools/gdb/kernel_auto.gdb
	@echo "directory ." >> tools/gdb/kernel_auto.gdb
	@echo "directory kernel/" >> tools/gdb/kernel_auto.gdb
	@echo "directory kernel/core/init/" >> tools/gdb/kernel_auto.gdb
	@echo "directory kernel/core/debug/" >> tools/gdb/kernel_auto.gdb
	@echo "directory kernel/drivers/video/" >> tools/gdb/kernel_auto.gdb
	@echo "# Start with assembly view" >> tools/gdb/kernel_auto.gdb
	@echo "tui enable" >> tools/gdb/kernel_auto.gdb
	@echo "layout asm" >> tools/gdb/kernel_auto.gdb
	@echo "focus cmd" >> tools/gdb/kernel_auto.gdb
	@echo "# Set breakpoint and auto-continue to C code" >> tools/gdb/kernel_auto.gdb
	@echo "break kernel_main" >> tools/gdb/kernel_auto.gdb
	@echo "echo === Auto-continuing to C source code ===" >> tools/gdb/kernel_auto.gdb
	@echo "continue" >> tools/gdb/kernel_auto.gdb
	@echo "# Switch to source view when we reach C code" >> tools/gdb/kernel_auto.gdb
	@echo "layout split" >> tools/gdb/kernel_auto.gdb
	@echo "refresh" >> tools/gdb/kernel_auto.gdb
	@echo "list" >> tools/gdb/kernel_auto.gdb
	@echo "echo" >> tools/gdb/kernel_auto.gdb
	@echo "echo === Now at C source code! ===" >> tools/gdb/kernel_auto.gdb
	@echo "echo Commands: step, next, continue, bt, list" >> tools/gdb/kernel_auto.gdb
	@echo "echo Use Ctrl+X+A to toggle TUI mode" >> tools/gdb/kernel_auto.gdb
	@echo ""
	@echo "Auto-navigating GDB script created: tools/gdb/kernel_auto.gdb"
	@echo "Usage: gdb -x tools/gdb/kernel_auto.gdb"

gdb-bootloader: debug-symbols ## Generate GDB script for bootloader debugging
	@echo "Creating GDB script for bootloader debugging (Stage1 + Stage2)..."
	@mkdir -p tools/gdb
	@echo "# Connect to QEMU" > tools/gdb/bootloader.gdb
	@echo "target remote :1234" >> tools/gdb/bootloader.gdb
	@echo "# Set 16-bit real mode architecture" >> tools/gdb/bootloader.gdb
	@echo "set architecture i8086" >> tools/gdb/bootloader.gdb
	@echo "# Load symbol files" >> tools/gdb/bootloader.gdb
	@echo "add-symbol-file $(STAGE1_ELF) 0x7c00" >> tools/gdb/bootloader.gdb
	@echo "add-symbol-file $(STAGE2_ELF) 0x7e00" >> tools/gdb/bootloader.gdb
	@echo "# Enable TUI mode with layout" >> tools/gdb/bootloader.gdb
	@echo "tui enable" >> tools/gdb/bootloader.gdb
	@echo "layout asm" >> tools/gdb/bootloader.gdb
	@echo "focus cmd" >> tools/gdb/bootloader.gdb
	@echo "# Set breakpoints at key locations" >> tools/gdb/bootloader.gdb
	@echo "hbreak *0x7c00" >> tools/gdb/bootloader.gdb
	@echo "hbreak *0x7e00" >> tools/gdb/bootloader.gdb
	@echo "hbreak *0x10000" >> tools/gdb/bootloader.gdb
	@echo "# Display breakpoints and symbols" >> tools/gdb/bootloader.gdb
	@echo "info breakpoints" >> tools/gdb/bootloader.gdb
	@echo "info files" >> tools/gdb/bootloader.gdb
	@echo "# Show current state" >> tools/gdb/bootloader.gdb
	@echo "info registers" >> tools/gdb/bootloader.gdb
	@echo "x/5i \$$pc" >> tools/gdb/bootloader.gdb
	@echo "# Debugging tips" >> tools/gdb/bootloader.gdb
	@echo "echo" >> tools/gdb/bootloader.gdb
	@echo "echo ==== Bootloader Debug Session ====" >> tools/gdb/bootloader.gdb
	@echo "echo Stage1 starts at 0x7c00" >> tools/gdb/bootloader.gdb
	@echo "echo Stage2 starts at 0x7e00" >> tools/gdb/bootloader.gdb
	@echo "echo Kernel starts at 0x10000" >> tools/gdb/bootloader.gdb
	@echo "echo Use 'continue' to run to first breakpoint" >> tools/gdb/bootloader.gdb
	@echo "echo Use 'stepi' to step one instruction" >> tools/gdb/bootloader.gdb
	@echo "echo Use 'info registers' to see register state" >> tools/gdb/bootloader.gdb
	@echo "echo ====================================" >> tools/gdb/bootloader.gdb
	@echo "GDB script created: tools/gdb/bootloader.gdb"

gdb-kernel: debug-symbols ## Generate GDB script for kernel debugging
	@echo "Creating GDB script for kernel debugging..."
	@mkdir -p tools/gdb
	@echo "set architecture i386" > tools/gdb/kernel_debug.gdb
	@echo "target remote :1234" >> tools/gdb/kernel_debug.gdb
	@echo "symbol-file $(KERNEL_ELF)" >> tools/gdb/kernel_debug.gdb
	@echo "# Enable TUI mode with source layout" >> tools/gdb/kernel_debug.gdb
	@echo "tui enable" >> tools/gdb/kernel_debug.gdb
	@echo "layout src" >> tools/gdb/kernel_debug.gdb
	@echo "focus cmd" >> tools/gdb/kernel_debug.gdb
	@echo "break kernel_main" >> tools/gdb/kernel_debug.gdb
	@echo "# Show breakpoints" >> tools/gdb/kernel_debug.gdb
	@echo "info breakpoints" >> tools/gdb/kernel_debug.gdb
	@echo "# Ready to debug - use 'continue' to start" >> tools/gdb/kernel_debug.gdb
	@echo "GDB script created: tools/gdb/kernel_debug.gdb"

gdb-bootloader-regs: debug-symbols ## Generate advanced bootloader GDB script with registers
	@echo "Creating advanced GDB script for bootloader debugging with registers..."
	@mkdir -p tools/gdb
	@echo "# Connect to QEMU" > tools/gdb/bootloader_regs.gdb
	@echo "target remote :1234" >> tools/gdb/bootloader_regs.gdb
	@echo "set architecture i8086" >> tools/gdb/bootloader_regs.gdb
	@echo "# Load symbol files" >> tools/gdb/bootloader_regs.gdb
	@echo "add-symbol-file $(STAGE1_ELF) 0x7c00" >> tools/gdb/bootloader_regs.gdb
	@echo "add-symbol-file $(STAGE2_ELF) 0x7e00" >> tools/gdb/bootloader_regs.gdb
	@echo "# Enable TUI with registers layout" >> tools/gdb/bootloader_regs.gdb
	@echo "tui enable" >> tools/gdb/bootloader_regs.gdb
	@echo "layout regs" >> tools/gdb/bootloader_regs.gdb
	@echo "focus cmd" >> tools/gdb/bootloader_regs.gdb
	@echo "hbreak *0x7c00" >> tools/gdb/bootloader_regs.gdb
	@echo "hbreak *0x7e00" >> tools/gdb/bootloader_regs.gdb
	@echo "hbreak *0x10000" >> tools/gdb/bootloader_regs.gdb
	@echo "info breakpoints" >> tools/gdb/bootloader_regs.gdb
	@echo "info files" >> tools/gdb/bootloader_regs.gdb
	@echo "GDB script with registers created: tools/gdb/bootloader_regs.gdb"

gdb-kernel-split: debug-symbols ## Generate advanced kernel GDB script with split layout
	@echo "Creating advanced GDB script for kernel debugging with split layout..."
	@mkdir -p tools/gdb
	@echo "set architecture i386" > tools/gdb/kernel_split.gdb
	@echo "target remote :1234" >> tools/gdb/kernel_split.gdb
	@echo "symbol-file $(KERNEL_ELF)" >> tools/gdb/kernel_split.gdb
	@echo "# Enable TUI with split layout (source + assembly)" >> tools/gdb/kernel_split.gdb
	@echo "tui enable" >> tools/gdb/kernel_split.gdb
	@echo "layout split" >> tools/gdb/kernel_split.gdb
	@echo "focus cmd" >> tools/gdb/kernel_split.gdb
	@echo "break kernel_main" >> tools/gdb/kernel_split.gdb
	@echo "info breakpoints" >> tools/gdb/kernel_split.gdb
	@echo "GDB script with split layout created: tools/gdb/kernel_split.gdb"

gdb-full-debug: debug-symbols ## Generate comprehensive bootloader-to-kernel debug script
	@echo "Creating comprehensive GDB script for bootloader-to-kernel debugging..."
	@mkdir -p tools/gdb
	@echo "# === BOOTLOADER PHASE ===" > tools/gdb/full_debug.gdb
	@echo "target remote :1234" >> tools/gdb/full_debug.gdb
	@echo "set architecture i8086" >> tools/gdb/full_debug.gdb
	@echo "add-symbol-file $(STAGE1_ELF) 0x7c00" >> tools/gdb/full_debug.gdb
	@echo "add-symbol-file $(STAGE2_ELF) 0x7e00" >> tools/gdb/full_debug.gdb
	@echo "tui enable" >> tools/gdb/full_debug.gdb
	@echo "layout asm" >> tools/gdb/full_debug.gdb
	@echo "focus cmd" >> tools/gdb/full_debug.gdb
	@echo "hbreak *0x7c00" >> tools/gdb/full_debug.gdb
	@echo "hbreak *0x7e00" >> tools/gdb/full_debug.gdb
	@echo "# === KERNEL TRANSITION POINT ===" >> tools/gdb/full_debug.gdb
	@echo "define switch-to-kernel" >> tools/gdb/full_debug.gdb
	@echo "  set architecture i386" >> tools/gdb/full_debug.gdb
	@echo "  add-symbol-file $(KERNEL_ELF) 0x10000" >> tools/gdb/full_debug.gdb
	@echo "  hbreak *0x10000" >> tools/gdb/full_debug.gdb
	@echo "  break kernel_main" >> tools/gdb/full_debug.gdb
	@echo "  layout split" >> tools/gdb/full_debug.gdb
	@echo "  echo" >> tools/gdb/full_debug.gdb
	@echo "  echo ==== SWITCHED TO KERNEL DEBUGGING ====" >> tools/gdb/full_debug.gdb
	@echo "  echo Now debugging in 32-bit protected mode" >> tools/gdb/full_debug.gdb
	@echo "  echo Breakpoints set at 0x10000 and kernel_main" >> tools/gdb/full_debug.gdb
	@echo "  echo Use 'continue' to proceed to kernel entry" >> tools/gdb/full_debug.gdb
	@echo "  echo =========================================" >> tools/gdb/full_debug.gdb
	@echo "  info breakpoints" >> tools/gdb/full_debug.gdb
	@echo "end" >> tools/gdb/full_debug.gdb
	@echo "# === KERNEL LOADING VERIFICATION ===" >> tools/gdb/full_debug.gdb
	@echo "define check-kernel-loaded" >> tools/gdb/full_debug.gdb
	@echo "  echo Checking if kernel was loaded at 0x10000..." >> tools/gdb/full_debug.gdb
	@echo "  x/10i 0x10000" >> tools/gdb/full_debug.gdb
	@echo "  echo" >> tools/gdb/full_debug.gdb
	@echo "  echo First 32 bytes of kernel memory:" >> tools/gdb/full_debug.gdb
	@echo "  x/32b 0x10000" >> tools/gdb/full_debug.gdb
	@echo "  echo" >> tools/gdb/full_debug.gdb
	@echo "  echo If you see all zeros or repeated 0x00 0x00, kernel didn't load!" >> tools/gdb/full_debug.gdb
	@echo "end" >> tools/gdb/full_debug.gdb
	@echo "# === DISK LOADING DEBUG ===" >> tools/gdb/full_debug.gdb
	@echo "define debug-disk-load" >> tools/gdb/full_debug.gdb
	@echo "  echo Setting breakpoint at disk loading section..." >> tools/gdb/full_debug.gdb
	@echo "  echo After disk read, check:" >> tools/gdb/full_debug.gdb
	@echo "  echo   x/10b 0x10000  - to see if kernel loaded" >> tools/gdb/full_debug.gdb
	@echo "  echo   info registers - to check carry flag for errors" >> tools/gdb/full_debug.gdb
	@echo "end" >> tools/gdb/full_debug.gdb
	@echo "# === INITIAL SETUP ===" >> tools/gdb/full_debug.gdb
	@echo "echo" >> tools/gdb/full_debug.gdb
	@echo "echo ==== BOOTLOADER-TO-KERNEL DEBUG SESSION ====" >> tools/gdb/full_debug.gdb
	@echo "echo Stage1: 0x7c00, Stage2: 0x7e00, Kernel: 0x10000" >> tools/gdb/full_debug.gdb
	@echo "echo" >> tools/gdb/full_debug.gdb
	@echo "echo Available commands:" >> tools/gdb/full_debug.gdb
	@echo "echo   check-kernel-loaded  - Verify if kernel loaded at 0x10000" >> tools/gdb/full_debug.gdb
	@echo "echo   debug-disk-load      - Debug disk loading process" >> tools/gdb/full_debug.gdb
	@echo "echo   switch-to-kernel     - Switch to kernel debugging mode" >> tools/gdb/full_debug.gdb
	@echo "echo" >> tools/gdb/full_debug.gdb
	@echo "echo When you reach the kernel jump point:" >> tools/gdb/full_debug.gdb
	@echo "echo   1. Type: check-kernel-loaded" >> tools/gdb/full_debug.gdb
	@echo "echo   2. If kernel loaded: switch-to-kernel" >> tools/gdb/full_debug.gdb
	@echo "echo   3. Then: continue" >> tools/gdb/full_debug.gdb
	@echo "echo =============================================" >> tools/gdb/full_debug.gdb
	@echo "info breakpoints" >> tools/gdb/full_debug.gdb
	@echo "GDB comprehensive debug script created: tools/gdb/full_debug.gdb"