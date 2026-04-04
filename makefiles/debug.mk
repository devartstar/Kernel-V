# ==============================================================================
# Debug Build Configuration & Advanced Debugging Features
# ==============================================================================

# Debug-specific disk images
DISK_DEBUG_IMG := $(BUILDDIR)/disk_debug.img

.PHONY: debug-symbols verify-symbols debug-stage1 debug-stage2 debug-bootloader debug-kernel
.PHONY: gdb-bootloader gdb-kernel gdb-bootloader-regs gdb-kernel-split gdb-full-debug
.PHONY: gdb-test-integration debug-test-integration connect-test-integration
.PHONY: connect-gdb connect-bootloader

# ------------- DEBUGGING -------------
#  1. Debug Build the module
#  2. Generate the dbg script
#  3. Start the gdb with debug script
#  ------------------------------------

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

debug-kernel: debug-symbols ## Debug kernel with appropriate test image
ifeq ($(CONFIG_TESTS_UNIT)$(CONFIG_TESTS_INTEGRATION), yy)
	@echo "Starting QEMU for kernel debugging (Combined Unit + Integration Tests)..."
	@echo "Using disk image: $(DISK_TEST_IMG)"
	@echo "Connect with: $(GDB) -x tools/gdb/kernel.gdb"
	$(MAKE) $(DISK_TEST_IMG)
	$(Q)$(QEMU) -drive format=raw,file=$(DISK_TEST_IMG) -s -S -display curses
else ifeq ($(CONFIG_TESTS_UNIT), y)
	@echo "Starting QEMU for kernel debugging (Unit Tests Only)..."
	@echo "Using disk image: $(DISK_UNIT_TEST_IMG)"
	@echo "Connect with: $(GDB) -x tools/gdb/kernel.gdb"
	$(MAKE) $(DISK_UNIT_TEST_IMG)
	$(Q)$(QEMU) -drive format=raw,file=$(DISK_UNIT_TEST_IMG) -s -S -display curses
else ifeq ($(CONFIG_TESTS_INTEGRATION), y)
	@echo "Starting QEMU for kernel debugging (Integration Tests Only)..."
	@echo "Using disk image: $(DISK_INTEGRATION_IMG)"
	@echo "Connect with: $(GDB) -x tools/gdb/kernel.gdb"
	$(MAKE) $(DISK_INTEGRATION_IMG)
	$(Q)$(QEMU) -drive format=raw,file=$(DISK_INTEGRATION_IMG) -s -S -display curses
else
	@echo "Starting QEMU for kernel debugging (Production Build)..."
	@echo "Using disk image: $(DISK_IMG)"
	@echo "Connect with: $(GDB) -x tools/gdb/kernel.gdb"
	$(MAKE) $(DISK_IMG)
	$(Q)$(QEMU) -drive format=raw,file=$(DISK_IMG) -s -S -display curses
endif

debug-test-integration: $(DISK_INTEGRATION_IMG) gdb-test-integration ## Debug integration tests with proper symbols
ifeq ($(CONFIG_TESTS_INTEGRATION), y)
	@echo "Starting QEMU for integration test debugging..."
	@echo "Connect with: $(GDB) -x tools/gdb/integration_debug.gdb"
	$(Q)$(QEMU) -drive format=raw,file=$(DISK_INTEGRATION_IMG) -s -S -display curses
else
	@echo "Integration tests not enabled!"
endif

# --- Advanced GDB Script Generation ---
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

gdb-test-integration: ## Generate GDB script for integration test debugging
ifeq ($(CONFIG_TESTS_INTEGRATION), y)
	@echo "Creating GDB script for integration test debugging..."
	@mkdir -p tools/gdb
	@echo "set architecture i386" > tools/gdb/integration_debug.gdb
	@echo "target remote :1234" >> tools/gdb/integration_debug.gdb
	@echo "symbol-file $(KERNEL_INTEGRATION_ELF)" >> tools/gdb/integration_debug.gdb
	@echo "# Enable TUI mode with source layout" >> tools/gdb/integration_debug.gdb
	@echo "tui enable" >> tools/gdb/integration_debug.gdb
	@echo "layout src" >> tools/gdb/integration_debug.gdb
	@echo "focus cmd" >> tools/gdb/integration_debug.gdb
	@echo "# Process debugging breakpoints" >> tools/gdb/integration_debug.gdb
	@echo "break kernel_main" >> tools/gdb/integration_debug.gdb
	@echo "break run_kernel_tests" >> tools/gdb/integration_debug.gdb
	@echo "break create_test_processes" >> tools/gdb/integration_debug.gdb
	@echo "break proc_create" >> tools/gdb/integration_debug.gdb
	@echo "break proc_create_kernel_main" >> tools/gdb/integration_debug.gdb
	@echo "break proc_exit" >> tools/gdb/integration_debug.gdb
	@echo "break proc_exit" >> tools/gdb/integration_debug.gdb
	@echo "break cleanup_terminated_processes" >> tools/gdb/integration_debug.gdb
	@echo "break yield" >> tools/gdb/integration_debug.gdb
	@echo "break scheduler_pick_next" >> tools/gdb/integration_debug.gdb
	@echo "break switch_to" >> tools/gdb/integration_debug.gdb
	@echo "# Process function breakpoints" >> tools/gdb/integration_debug.gdb
	@echo "break my_test_proc" >> tools/gdb/integration_debug.gdb
	@echo "break preemptive_proc" >> tools/gdb/integration_debug.gdb
	@echo "break my_sleep_proc" >> tools/gdb/integration_debug.gdb
	@echo "break timer.c:46" >> tools/gdb/integration_debug.gdb
	@echo '# Process inspection commands' >> tools/gdb/integration_debug.gdb
	@echo 'define show-processes' >> tools/gdb/integration_debug.gdb
	@echo '  printf "\n=== PROCESS LIST ===\n"' >> tools/gdb/integration_debug.gdb
	@echo '  set $$p = ready_list_head' >> tools/gdb/integration_debug.gdb
	@echo '  while $$p' >> tools/gdb/integration_debug.gdb
	@echo '    printf "PID: %d, Name: %s, State: %d\n", $$p->pid, $$p->name, $$p->state' >> tools/gdb/integration_debug.gdb
	@echo '    printf "  EIP: 0x%08x, ESP: 0x%08x\n", $$p->context.eip, $$p->context.esp' >> tools/gdb/integration_debug.gdb
	@echo '    set $$p = $$p->next' >> tools/gdb/integration_debug.gdb
	@echo '  end' >> tools/gdb/integration_debug.gdb
	@echo '  printf "\n"' >> tools/gdb/integration_debug.gdb
	@echo 'end' >> tools/gdb/integration_debug.gdb
	@echo '# Show breakpoints' >> tools/gdb/integration_debug.gdb
	@echo 'info breakpoints' >> tools/gdb/integration_debug.gdb
	@echo '# Ready to debug processes' >> tools/gdb/integration_debug.gdb
	@echo 'printf "\n=== INTEGRATION TEST DEBUG SESSION ===\n"' >> tools/gdb/integration_debug.gdb
	@echo 'printf "Available commands:\n"' >> tools/gdb/integration_debug.gdb
	@echo 'printf "  show-processes - List all processes\n"' >> tools/gdb/integration_debug.gdb
	@echo 'printf "  continue       - Run to next breakpoint\n"' >> tools/gdb/integration_debug.gdb
	@echo 'printf "==========================================\n\n"' >> tools/gdb/integration_debug.gdb
	@echo "GDB integration test script created: tools/gdb/integration_debug.gdb"
else
	@echo "Integration tests not enabled. Run 'make menuconfig' and enable CONFIG_TESTS_INTEGRATION"
endif

# ---  GDB Connection Targets ---
connect-gdb: gdb-kernel ## Connect GDB to running QEMU (use existing kernel.gdb)
	@echo "Connecting GDB to running QEMU session..."
	@echo "Make sure QEMU is running in another terminal!"
	$(GDB) -x tools/gdb/kernel_debug.gdb

connect-bootloader: gdb-bootloader ## Connect GDB to running QEMU for bootloader
	@echo "Connecting GDB to running QEMU session for bootloader..."
	@echo "Make sure QEMU is running in another terminal!"
	$(GDB) -x tools/gdb/bootloader.gdb

connect-test-integration: gdb-test-integration ## Connect GDB to running integration test QEMU
ifeq ($(CONFIG_TESTS_INTEGRATION), y)
	@echo "Connecting GDB to running integration test QEMU session..."
	@echo "Make sure QEMU is running integration test image!"
	$(GDB) -x tools/gdb/integration_debug.gdb
else
	@echo "Integration tests not enabled!"
endif
