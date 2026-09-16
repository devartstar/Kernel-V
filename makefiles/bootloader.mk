# ==================================================================
# Bootloader Build
# ==================================================================

# --- Bootloader Source Files ---
STAGE1_SRC := $(BOOTDIR)/stage1.asm
STAGE2_SRC := $(BOOTDIR)/stage2.asm

# --- Bootloader Output Files ---
STAGE1_BIN := $(BUILD_BOOT)/stage1.bin
STAGE2_BIN := $(BUILD_BOOT)/stage2.bin
STAGE1_ELF := $(BUILD_BOOT)/stage1.elf
STAGE2_ELF := $(BUILD_BOOT)/stage2.elf
STAGE2_KERNEL_INC := $(BUILD_BOOT)/stage2_kernel.inc
STAGE1_STAGE2_INC := $(BUILD_BOOT)/stage1_stage2.inc

# Select the kernel payload actually written to the active disk image.
ifeq ($(CONFIG_BUILD_TEST), y)
ifeq ($(CONFIG_TESTS_UNIT)$(CONFIG_TESTS_INTEGRATION), yy)
STAGE2_PAYLOAD_BIN := $(BUILD_TEST)/kernel_test.bin
else ifeq ($(CONFIG_TESTS_UNIT), y)
STAGE2_PAYLOAD_BIN := $(BUILD_TEST)/kernel_unit_test.bin
else ifeq ($(CONFIG_TESTS_INTEGRATION), y)
STAGE2_PAYLOAD_BIN := $(BUILD_TEST)/kernel_integration.bin
else
STAGE2_PAYLOAD_BIN := $(KERNEL_BIN)
endif
else
STAGE2_PAYLOAD_BIN := $(KERNEL_BIN)
endif

# --- Bootloader Object Files ---
STAGE1_OBJ := $(BUILD_BOOT)/stage1.o
STAGE2_OBJ := $(BUILD_BOOT)/stage2.o

.PHONY: bootloader clean-bootloader

bootloader: $(STAGE1_BIN) $(STAGE2_BIN) $(STAGE1_ELF) $(STAGE2_ELF)

# --- Stage 1 ---
$(STAGE1_STAGE2_INC): $(STAGE2_BIN) | $(BUILD_BOOT)
	$(Q){ \
		echo "; Auto-generated. Do not edit."; \
		bytes=$$(stat -c%s "$(STAGE2_BIN)"); \
		sectors=$$(( (bytes + 511) / 512 )); \
		echo "%define STAGE2_TOTAL_SECTORS $$sectors"; \
	} > $@

$(STAGE1_BIN): $(STAGE1_SRC) $(STAGE1_STAGE2_INC) | $(BUILD_BOOT)
	$(ECHO) "  ASM     $@"
	$(Q)$(NASM) -I$(BUILD_BOOT)/ -f bin $< -o $@

$(STAGE1_OBJ): $(STAGE1_SRC) $(STAGE1_STAGE2_INC) | $(BUILD_BOOT)
	$(ECHO) "  ASM     $@"
	$(Q)$(NASM) -I$(BUILD_BOOT)/ -f elf32 -g -DELF_BUILD $< -o $@

$(STAGE1_ELF): $(STAGE1_OBJ) | $(BUILD_BOOT)
	$(ECHO) "  LD      $@"
	$(Q)$(LD) -m elf_i386 -Ttext $(STAGE1_LOAD_ADDR) --oformat=elf32-i386 $< -o $@

# --- Stage 2 ---
$(STAGE2_KERNEL_INC): $(STAGE2_PAYLOAD_BIN) | $(BUILD_BOOT)
	$(Q){ \
		echo "; Auto-generated. Do not edit."; \
		echo "%define KERNEL_START_LBA $(KERNEL_SECTOR)"; \
		if [ ! -s "$(STAGE2_PAYLOAD_BIN)" ]; then \
			echo "Missing or empty stage2 payload: $(STAGE2_PAYLOAD_BIN)" >&2; \
			exit 1; \
		fi; \
		bytes=$$(stat -c%s "$(STAGE2_PAYLOAD_BIN)"); \
		sectors=$$(( (bytes + 511) / 512 )); \
		if [ $$sectors -le 0 ]; then \
			echo "Computed invalid kernel sector count: $$sectors" >&2; \
			exit 1; \
		fi; \
		echo "%define KERNEL_TOTAL_SECTORS $$sectors"; \
	} > $@

$(STAGE2_BIN): $(STAGE2_SRC) $(STAGE2_KERNEL_INC) | $(BUILD_BOOT)
	$(ECHO) "  ASM     $@"
	$(Q)$(NASM) -I$(BUILD_BOOT)/ -f bin $< -o $@

$(STAGE2_OBJ): $(STAGE2_SRC) $(STAGE2_KERNEL_INC) | $(BUILD_BOOT)
	$(ECHO) "  ASM     $@"
	$(Q)$(NASM) -I$(BUILD_BOOT)/ -f elf32 -g -DELF_BUILD $< -o $@

$(STAGE2_ELF): $(STAGE2_OBJ) | $(BUILD_BOOT)
	$(ECHO) "  LD      $@"
	$(Q)$(LD) -m elf_i386 -Ttext $(STAGE2_LOAD_ADDR) --oformat=elf32-i386 $< -o $@

# --- Directory creation ---
$(BUILD_BOOT):
	$(Q)mkdir -p $@

clean-bootloader:
	$(Q)rm -rf $(BUILD_BOOT)