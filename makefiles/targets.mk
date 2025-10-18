# ==============================================================================
# Main Build Targets
# ==============================================================================

# --- Main disk image ---
DISK_IMG := $(BUILDDIR)/disk.img

.PHONY: all build run debug clean distclean install format check help
.PHONY: debug-symbols verify-symbols debug-stage1 debug-stage2 debug-bootloader debug-kernel
.PHONY: gdb-bootloader gdb-kernel gdb-bootloader-regs gdb-kernel-split gdb-full-debug

all: $(DISK_IMG) ## Build complete system

build: all ## Alias for all

# --- Disk Image Creation ---
$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) | $(BUILDDIR)
	$(ECHO) "  DISK    $@"
	$(Q)$(DD) if=/dev/zero of=$@ bs=1K count=1440 2>/dev/null
	$(Q)$(DD) if=$(STAGE1_BIN) of=$@ bs=512 seek=$(STAGE1_SECTOR) conv=notrunc 2>/dev/null
	$(Q)$(DD) if=$(STAGE2_BIN) of=$@ bs=512 seek=$(STAGE2_SECTOR) conv=notrunc 2>/dev/null  
	$(Q)$(DD) if=$(KERNEL_BIN) of=$@ bs=512 seek=$(KERNEL_SECTOR) conv=notrunc 2>/dev/null

# --- Directory Creation ---
$(BUILDDIR):
	$(Q)mkdir -p $@

# --- Run Targets ---
run: $(DISK_IMG) ## Build and run in QEMU
	$(ECHO) "Starting QEMU..."
	$(Q)$(QEMU) -drive format=raw,file=$< -display curses

debug: $(DISK_IMG) $(STAGE1_ELF) $(STAGE2_ELF) $(KERNEL_ELF) ## Run with GDB support
	$(ECHO) "Starting QEMU with GDB support..."
	$(ECHO) "Connect with: $(GDB) -x tools/gdb/kernel.gdb"
	$(Q)$(QEMU) -drive format=raw,file=$< -s -S -display curses

# --- Maintenance Targets ---
clean: clean-bootloader clean-kernel clean-tests ## Clean all build artifacts
	$(Q)rm -f $(DISK_IMG)

distclean: clean ## Complete clean including generated files
	$(Q)rm -rf $(BUILDDIR)
	$(Q)rm -f $(PROC_OFFSET_HDR) $(PROC_OFFSET_GEN)

format: ## Format source code
	$(Q)find $(KERNDIR) -name "*.c" -o -name "*.h" | xargs clang-format -i

check: ## Static analysis
	$(Q)cppcheck --enable=all --suppress=missingIncludeSystem $(KERNDIR)

install: $(DISK_IMG) ## Install to USB device (requires DEVICE variable)
ifndef DEVICE
	$(error Please specify DEVICE=/dev/sdX)
endif
	$(ECHO) "Installing to $(DEVICE)..."
	$(Q)sudo $(DD) if=$< of=$(DEVICE) bs=1M

# --- Information Targets ---
info: ## Show build configuration
	$(ECHO) "Build Configuration:"
	$(ECHO) "  Project: $(PROJECT_NAME) v$(VERSION)"
	$(ECHO) "  Build Type: $(BUILD_TYPE)"
	$(ECHO) "  Build Date: $(BUILD_DATE)"
	$(ECHO) "  Target: $(TARGET)"
	$(ECHO) "  CC: $(CC)"
	$(ECHO) "  CFLAGS: $(CFLAGS)"
	$(ECHO) "  Directories:"
	$(ECHO) "    Source: $(KERNDIR)"
	$(ECHO) "    Build: $(BUILDDIR)" 
	$(ECHO) "    Include: $(INCDIR)"

sizes: $(KERNEL_BIN) $(STAGE1_BIN) $(STAGE2_BIN) ## Show component sizes
	$(ECHO) "Component Sizes:"
	$(ECHO) "  Stage1:  $$(stat -c%s $(STAGE1_BIN)) bytes"
	$(ECHO) "  Stage2:  $$(stat -c%s $(STAGE2_BIN)) bytes"  
	$(ECHO) "  Kernel:  $$(stat -c%s $(KERNEL_BIN)) bytes"
	$(ECHO) "  Total:   $$(($$(stat -c%s $(STAGE1_BIN)) + $$(stat -c%s $(STAGE2_BIN)) + $$(stat -c%s $(KERNEL_BIN)))) bytes"

# --- Enhanced Help System ---
commands-help: ## Show this help message
	@echo "Kernel-V Build Commands:"
	@echo ""
	@echo "Build targets:"
	@echo "  make                    - Build PRODUCTION kernel (no tests, minimal size)"
	@echo "  make clean             - Clean build directory"
	@echo "  make run               - Build and run PRODUCTION kernel in QEMU"
	@echo "  make BUILD_TYPE=debug  - Build with debug symbols"
	@echo "  make BUILD_TYPE=test   - Build with test suite"
	@echo ""
	@echo "Test targets:"
	@echo "  make test              - Build and run kernel with FULL test suite (legacy)"
	@echo "  make test-unit         - Build and run unit tests (panik, printk)"
	@echo "  make test-integration  - Build and run integration tests (process tests)"
	@echo "  make test-all          - Build and run complete test suite"
	@echo ""
	@echo "Debug targets:"
	@echo "  make debug             - Run with GDB server (kernel debugging)"
	@echo "  make debug-stage1      - Debug Stage 1 bootloader"
	@echo "  make debug-stage2      - Debug Stage 2 bootloader" 
	@echo "  make debug-bootloader  - Debug both bootloader stages"
	@echo "  make debug-kernel      - Debug kernel only"
	@echo "  make verify-symbols    - Verify debug symbols are present"
	@echo ""
	@echo "GDB Helper Scripts:"
	@echo "  make gdb-bootloader       - Generate bootloader GDB script"
	@echo "  make gdb-kernel           - Generate kernel GDB script"
	@echo "  make gdb-bootloader-regs  - Generate bootloader script with registers"
	@echo "  make gdb-kernel-split     - Generate kernel script with split layout"
	@echo "  make gdb-full-debug       - Generate comprehensive debug script"
	@echo ""
	@echo "Build variants explained:"
	@echo "  Production kernel    = Core functionality only (minimal footprint)"
	@echo "  Debug kernel        = Production + debug symbols and features"
	@echo "  Test kernel         = Debug + all test files enabled"
	@echo "  Unit tests          = Unit tests only (panik, printk tests)"
	@echo "  Integration tests   = Integration tests (process tests)"
	@echo ""
	@echo "Key differences:"
	@echo "  'make run'          = No test processes, smaller binary, production code"
	@echo "  'make test'         = Includes test processes, larger binary, debug features"
	@echo "  'make test-unit'    = Only unit tests enabled"
	@echo "  'make test-all'     = All tests enabled"
	@echo ""
	@echo "Info targets:"
	@echo "  make info    - Show build configuration"
	@echo "  make sizes   - Show component sizes"
	@echo "  make help    - Show this help"