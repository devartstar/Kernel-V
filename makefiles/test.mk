# ==============================================================================
# Test Build Configuration
# ==============================================================================

# User program for integration tests
USERPROG_ASM 				:= $(KERN_ARCH_DIR)/user/userprog.asm
USERPROG_BIN 				:= $(BUILD_TEST)/userprog.bin
USERPROG_OBJ 				:= $(BUILD_TEST)/userprog.o

# Test-specific sources
ifeq ($(CONFIG_TESTS_UNIT), y)
UNIT_TEST_SOURCES 			:= $(shell find $(TESTDIR)/unit -type f -name "*.c")
UNIT_TEST_OBJS 				:= $(patsubst $(KERNDIR)/%.c,$(BUILD_TEST)/%.o,$(UNIT_TEST_SOURCES))
else
UNIT_TEST_SOURCES 			:=
UNIT_TEST_OBJS 				:=
endif

ifeq ($(CONFIG_TESTS_INTEGRATION), y)
INTEGRATION_TEST_SOURCES 	:= $(shell find $(TESTDIR)/integration -type f -name "*.c") \
							   $(shell find $(TESTDIR)/interrupt -type f -name "*.c" 2>/dev/null || true)
INTEGRATION_TEST_OBJS 		:= $(patsubst $(KERNDIR)/%.c,$(BUILD_TEST)/%.o,$(INTEGRATION_TEST_SOURCES)) $(USERPROG_OBJ)
else
INTEGRATION_TEST_SOURCES 	:=
INTEGRATION_TEST_OBJS 		:=
endif

TEST_RUNNER_SOURCE 			:= $(TESTDIR)/test_runner.c
TEST_RUNNER_OBJECT 			:= $(BUILD_TEST)/test_runner.o

ALL_TEST_OBJS 				:= $(UNIT_TEST_OBJS) $(INTEGRATION_TEST_OBJS) $(TEST_RUNNER_OBJECT)

# Core kernel objects for tests (excluding tests themselves)
KERNEL_CORE_TEST_OBJS := $(BUILD_TEST)/kernel_entry.o \
						 $(patsubst $(KERNDIR)/%.c,$(BUILD_TEST)/%.o,$(shell find $(KERNDIR) -name "*.c" -not -path "$(TESTDIR)/*" -not -name "*_generator.c")) \
						 $(patsubst $(KERNDIR)/%.asm,$(BUILD_TEST)/%.o,$(shell find $(KERNDIR) -name "*.asm" -not -path "$(BOOTDIR)/*" -not -name "kernel_entry.asm"))

# Test kernels - UNIT, INTEGRATION, FULL
KERNEL_TEST_ELF 			:= $(BUILD_TEST)/kernel_test.elf
KERNEL_TEST_BIN 			:= $(BUILD_TEST)/kernel_test.bin
KERNEL_UNIT_TEST_ELF 		:= $(BUILD_TEST)/kernel_unit_test.elf
KERNEL_UNIT_TEST_BIN 		:= $(BUILD_TEST)/kernel_unit_test.bin
KERNEL_INTEGRATION_ELF 		:= $(BUILD_TEST)/kernel_integration.elf  
KERNEL_INTEGRATION_BIN 		:= $(BUILD_TEST)/kernel_integration.bin

# Test disk images - UNIT, INTEGRATION, FULL
DISK_TEST_IMG 				:= $(BUILD_TEST)/disk_test.img
DISK_UNIT_TEST_IMG 			:= $(BUILD_TEST)/disk_unit_test.img
DISK_INTEGRATION_IMG 		:= $(BUILD_TEST)/disk_integration.img
DISK_FULL_TEST_IMG 			:= $(BUILD_TEST)/disk_full_test.img


.PHONY: test-build test-run tests test-unit test-integration test-combined clean-tests

### BUILD PHASE ###

ifeq ($(CONFIG_BUILD_TEST), y)
test-build: ## Build test kernels based on enabled configs
	@echo "======================================================================"
	@echo "                    KERNEL-V TEST BUILD"
	@echo "======================================================================"
ifeq ($(CONFIG_TESTS_UNIT)$(CONFIG_TESTS_INTEGRATION), yy)
	@echo "==> Building Combined Unit and Integration Test Kernel"
	@echo "    • Unit Tests: panik, printk functionality"  
	@echo "    • Integration Tests: process management"
	$(MAKE) $(DISK_TEST_IMG)
	@echo "    ✓ Combined test kernel built: $(KERNEL_COMBINED_TEST_BIN)"
else ifeq ($(CONFIG_TESTS_UNIT), y)
	@echo "==> Building Unit Test Kernel Only"
	@echo "    • Unit Tests: panik, printk functionality"
	$(MAKE) $(DISK_UNIT_TEST_IMG)
	@echo "    ✓ Unit test kernel built: $(KERNEL_UNIT_TEST_BIN)"
else ifeq ($(CONFIG_TESTS_INTEGRATION), y)
	@echo "==> Building Integration Test Kernel Only"
	@echo "    • Integration Tests: process management"
	$(MAKE) $(DISK_INTEGRATION_IMG)
	@echo "    ✓ Integration test kernel built: $(KERNEL_INTEGRATION_BIN)"
else
	@echo "==> No tests enabled!"
	@echo "    Please enable tests via 'make menuconfig':"
	@echo "    • CONFIG_BUILD_TEST=y (required)"
	@echo "    • CONFIG_TESTS_UNIT=y (for unit tests)"
	@echo "    • CONFIG_TESTS_INTEGRATION=y (for integration tests)"
	@false
endif
	@echo "======================================================================"

### RUN PHASE ###

test-run: ## Run the built test kernel
	@echo "======================================================================"
	@echo "                    KERNEL-V TEST RUNNER"
	@echo "======================================================================"
ifeq ($(CONFIG_TESTS_UNIT)$(CONFIG_TESTS_INTEGRATION), yy)
	@echo "==> Running Combined Test Suite"
	@echo "    • Tests will run sequentially in single QEMU session"
	@echo "    • Exit QEMU (Ctrl+Alt+G, then Ctrl+C) when tests complete"
	@echo ""
	$(Q)$(QEMU) -drive format=raw,file=$(DISK_TEST_IMG) -serial file:serial.log -display curses
else ifeq ($(CONFIG_TESTS_UNIT), y)
	@echo "==> Running Unit Tests"
	@echo "    • Testing: panik, printk functionality" 
	@echo ""
	$(Q)$(QEMU) -drive format=raw,file=$(DISK_UNIT_TEST_IMG) -serial file:serial.log -display curses
else ifeq ($(CONFIG_TESTS_INTEGRATION), y)
	@echo "==> Running Integration Tests"
	@echo "    • Testing: process management"
	@echo ""
	$(Q)$(QEMU) -drive format=raw,file=$(DISK_INTEGRATION_IMG) -serial file:serial.log -display curses
else
	@echo "==> No tests enabled or built!"
	@echo "    Run 'make test-build' first"
	@false
endif
	@echo "======================================================================"

### COMBINED COMMAND ###

tests: test-build test-run ## Build and run tests (combined command)

endif

### DISK IMAGES ###
ifeq ($(CONFIG_BUILD_TEST), y)

$(DISK_TEST_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_TEST_BIN) | $(BUILD_TEST)
	$(ECHO) "  DISK    $@"
	$(Q)$(DD) if=/dev/zero of=$@ bs=1K count=1440 2>/dev/null
	$(Q)$(DD) if=$(STAGE1_BIN) of=$@ bs=512 seek=$(STAGE1_SECTOR) conv=notrunc 2>/dev/null
	$(Q)$(DD) if=$(STAGE2_BIN) of=$@ bs=512 seek=$(STAGE2_SECTOR) conv=notrunc 2>/dev/null
	$(Q)$(DD) if=$(KERNEL_TEST_BIN) of=$@ bs=512 seek=$(KERNEL_SECTOR) conv=notrunc 2>/dev/null

ifeq ($(CONFIG_TESTS_UNIT), y)
$(DISK_UNIT_TEST_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_UNIT_TEST_BIN) | $(BUILD_TEST)
	$(ECHO) "  DISK    $@"
	$(Q)$(DD) if=/dev/zero of=$@ bs=1K count=1440 2>/dev/null
	$(Q)$(DD) if=$(STAGE1_BIN) of=$@ bs=512 seek=$(STAGE1_SECTOR) conv=notrunc 2>/dev/null
	$(Q)$(DD) if=$(STAGE2_BIN) of=$@ bs=512 seek=$(STAGE2_SECTOR) conv=notrunc 2>/dev/null
	$(Q)$(DD) if=$(KERNEL_UNIT_TEST_BIN) of=$@ bs=512 seek=$(KERNEL_SECTOR) conv=notrunc 2>/dev/null
endif

ifeq ($(CONFIG_TESTS_INTEGRATION), y)
$(DISK_INTEGRATION_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_INTEGRATION_BIN) | $(BUILD_TEST)
	$(ECHO) "  DISK    $@"
	$(Q)$(DD) if=/dev/zero of=$@ bs=1K count=1440 2>/dev/null
	$(Q)$(DD) if=$(STAGE1_BIN) of=$@ bs=512 seek=$(STAGE1_SECTOR) conv=notrunc 2>/dev/null
	$(Q)$(DD) if=$(STAGE2_BIN) of=$@ bs=512 seek=$(STAGE2_SECTOR) conv=notrunc 2>/dev/null
	$(Q)$(DD) if=$(KERNEL_INTEGRATION_BIN) of=$@ bs=512 seek=$(KERNEL_SECTOR) conv=notrunc 2>/dev/null
endif

endif

### COMPILE AND GENERATE OBJECTS ###

ifeq ($(CONFIG_BUILD_TEST), y)

# Special rule for test kernel entry point
$(BUILD_TEST)/kernel_entry.o: $(KERN_ARCH_DIR)/boot/kernel_entry.asm $(PROC_OFFSET_HDR) | $(BUILD_TEST)
	$(ECHO) "  ASM-TEST $@"
	$(Q)$(NASM) $(NASMFLAGS) $< -o $@

# Pattern rule for test objects with proper flags
$(BUILD_TEST)/%.o: $(KERNDIR)/%.c | $(BUILD_TEST)
	$(ECHO) "  CC-TEST $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_TEST)/%.o: $(KERNDIR)/%.asm $(PROC_OFFSET_HDR) | $(BUILD_TEST)
	$(ECHO) "  ASM-TEST $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(NASM) $(NASMFLAGS) $< -o $@

$(BUILD_TEST)/%.o: $(KERNDIR)/tests/%.c | $(BUILD_TEST)
	$(ECHO) "  CC-TEST $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

# Only compile unit tests if they're enabled
ifeq ($(CONFIG_TESTS_UNIT), y)
$(BUILD_TEST)/%.o: $(KERNDIR)/tests/unit/%.c | $(BUILD_TEST)
	$(ECHO) "  CC-TEST $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -DUNIT_TESTS=1 -c $< -o $@
endif

# Only compile integration tests if they're enabled
ifeq ($(CONFIG_TESTS_INTEGRATION), y)
$(BUILD_TEST)/%.o: $(KERNDIR)/tests/integration/%.c | $(BUILD_TEST)
	$(ECHO) "  CC-TEST $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -DINTEGRATION_TEST=1 -c $< -o $@

$(BUILD_TEST)/%.o: $(KERNDIR)/tests/interrupt/%.c | $(BUILD_TEST)
	$(ECHO) "  CC-TEST $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -DINTEGRATION_TEST=1 -c $< -o $@

# User program build rules
$(USERPROG_BIN): $(USERPROG_ASM) | $(BUILD_TEST)
	$(ECHO) "  ASM-USER $@"
	$(Q)$(NASM) -f bin $< -o $@

$(USERPROG_OBJ): $(USERPROG_BIN) | $(BUILD_TEST)
	$(ECHO) "  OBJCOPY $@"
	$(Q)(cd $(BUILD_TEST) && \
	 $(OBJCOPY) -I binary -O elf32-i386 -B i386 \
		--rename-section .data=.rodata,contents,alloc,load,readonly,data \
		--redefine-sym _binary_userprog_bin_start=_binary_userprog_start \
		--redefine-sym _binary_userprog_bin_end=_binary_userprog_end \
		userprog.bin userprog.o)
endif

endif

### LINK: FULL TESTS ###

ifeq ($(CONFIG_BUILD_TEST), y)

$(KERNEL_TEST_ELF): $(KERNEL_CORE_TEST_OBJS) $(ALL_TEST_OBJS) $(KERNEL_LD) | $(BUILD_TEST)
	$(ECHO) "  LD-TEST $@"
	$(Q)$(LD) $(LDFLAGS) -T $(KERNEL_LD) -o $@ $(KERNEL_CORE_TEST_OBJS) $(ALL_TEST_OBJS) -nostdlib

$(KERNEL_TEST_BIN): $(KERNEL_TEST_ELF) | $(BUILD_TEST)
	$(ECHO) "  OBJCOPY $@"
	$(Q)$(OBJCOPY) -O binary $< $@

test-all: $(DISK_TEST_IMG) ## Legacy test build (full test suite)
	@echo "Running legacy test suite..."
	$(Q)$(QEMU) -drive format=raw,file=$< -display curses

endif


### LINK UNIT TESTS ###

ifeq ($(CONFIG_TESTS_UNIT), y)

$(KERNEL_UNIT_TEST_ELF): $(KERNEL_CORE_TEST_OBJS) $(UNIT_TEST_OBJS) $(TEST_RUNNER_OBJECT) $(KERNEL_LD) | $(BUILD_TEST)
	$(ECHO) "  LD-TEST $@"
	$(Q)$(LD) $(LDFLAGS) -T $(KERNEL_LD) -o $@ $(KERNEL_CORE_TEST_OBJS) $(UNIT_TEST_OBJS) $(TEST_RUNNER_OBJECT) -nostdlib

$(KERNEL_UNIT_TEST_BIN): $(KERNEL_UNIT_TEST_ELF) | $(BUILD_TEST)
	$(ECHO) "  OBJCOPY $@"
	$(Q)$(OBJCOPY) -O binary $< $@

test-unit: $(DISK_UNIT_TEST_IMG) ## Run unit tests (panik, printk)
	@echo "Running unit tests (panik, printk)..."
	$(Q)$(QEMU) -drive format=raw,file=$< -display curses

endif


### LINK INTEGRATION TESTS ###

ifeq ($(CONFIG_TESTS_INTEGRATION), y)

$(KERNEL_INTEGRATION_ELF): $(KERNEL_CORE_TEST_OBJS) $(INTEGRATION_TEST_OBJS) $(TEST_RUNNER_OBJECT) $(KERNEL_LD) | $(BUILD_TEST)
	$(ECHO) "  LD-TEST $@"
	$(Q)$(LD) $(LDFLAGS) -T $(KERNEL_LD) -o $@ $(KERNEL_CORE_TEST_OBJS) $(INTEGRATION_TEST_OBJS) $(TEST_RUNNER_OBJECT) -nostdlib

$(KERNEL_INTEGRATION_BIN): $(KERNEL_INTEGRATION_ELF) | $(BUILD_TEST)
	$(ECHO) "  OBJCOPY $@"
	$(Q)$(OBJCOPY) -O binary $< $@

test-integration: $(DISK_INTEGRATION_IMG) ## Run integration tests (process tests)
	@echo "Running integration tests (process tests)..."
	$(Q)$(QEMU) -drive format=raw,file=$< -display curses

endif


# Directory creation
$(BUILD_TEST):
	$(Q)mkdir -p $@
	$(Q)mkdir -p $(BUILD_TEST)/unit
	$(Q)mkdir -p $(BUILD_TEST)/proc
	$(Q)mkdir -p $(BUILD_TEST)/tests/unit
	$(Q)mkdir -p $(BUILD_TEST)/tests/integration

clean-tests: ## Clean test artifacts
	$(Q)rm -rf $(BUILD_TEST)
