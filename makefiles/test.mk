# ==============================================================================
# Test Build Configuration
# ==============================================================================

# Test-specific sources
UNIT_TEST_SOURCES 			:= $(shell find $(TESTDIR)/unit -type f -name "*.c")
INTEGRATION_TEST_SOURCES 	:= $(shell find $(TESTDIR)/integration -type f -name "*.c")
TEST_RUNNER_SOURCE 			:= $(TESTDIR)/test_runner.c

# Test objects
UNIT_TEST_OBJS 				:= $(patsubst $(KERNDIR)/%.c,$(BUILD_TEST)/%.o,$(UNIT_TEST_SOURCES))
INTEGRATION_TEST_OBJS 		:= $(patsubst $(KERNDIR)/%.c,$(BUILD_TEST)/%.o,$(INTEGRATION_TEST_SOURCES))
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


.PHONY: tests test test-unit test-integration test-all clean-tests

ifeq ($(CONFIG_BUILD_TEST), y)
tests: test-all ## Build all tests
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
	$(Q)$(CC) $(CFLAGS) -DKERNEL_TESTS -c $< -o $@

$(BUILD_TEST)/%.o: $(KERNDIR)/%.asm $(PROC_OFFSET_HDR) | $(BUILD_TEST)
	$(ECHO) "  ASM-TEST $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(NASM) $(NASMFLAGS) $< -o $@

$(BUILD_TEST)/%.o: $(KERNDIR)/tests/%.c | $(BUILD_TEST)
	$(ECHO) "  CC-TEST $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -DKERNEL_TESTS -c $< -o $@

$(BUILD_TEST)/%.o: $(KERNDIR)/tests/unit/%.c | $(BUILD_TEST)
	$(ECHO) "  CC-TEST $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -DKERNEL_TESTS -DUNIT_TESTS -c $< -o $@

$(BUILD_TEST)/%.o: $(KERNDIR)/tests/proc/%.c | $(BUILD_TEST)
	$(ECHO) "  CC-TEST $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -DKERNEL_TESTS -DPROC_TESTS -c $< -o $@

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
	$(Q)mkdir -p $(BUILD_TEST)/tests/proc

clean-tests: ## Clean test artifacts
	$(Q)rm -rf $(BUILD_TEST)
