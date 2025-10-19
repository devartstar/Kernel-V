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

# --- Bootloader Object Files ---
STAGE1_OBJ := $(BUILD_BOOT)/stage1.o
STAGE2_OBJ := $(BUILD_BOOT)/stage2.o

.PHONY: bootloader clean-bootloader

bootloader: $(STAGE1_BIN) $(STAGE2_BIN) $(STAGE1_ELF) $(STAGE2_ELF)

# --- Stage 1 ---
$(STAGE1_BIN): $(STAGE1_SRC) | $(BUILD_BOOT)
	$(ECHO) "  ASM     $@"
	$(Q)$(NASM) -f bin $< -o $@

$(STAGE1_OBJ): $(STAGE1_SRC) | $(BUILD_BOOT)
	$(ECHO) "  ASM     $@"
	$(Q)$(NASM) -f elf32 -g -DELF_BUILD $< -o $@

$(STAGE1_ELF): $(STAGE1_OBJ) | $(BUILD_BOOT)
	$(ECHO) "  LD      $@"
	$(Q)$(LD) -m elf_i386 -Ttext $(STAGE1_LOAD_ADDR) --oformat=elf32-i386 $< -o $@

# --- Stage 2 ---
$(STAGE2_BIN): $(STAGE2_SRC) | $(BUILD_BOOT)
	$(ECHO) "  ASM     $@"
	$(Q)$(NASM) -f bin $< -o $@

$(STAGE2_OBJ): $(STAGE2_SRC) | $(BUILD_BOOT)
	$(ECHO) "  ASM     $@"
	$(Q)$(NASM) -f elf32 -g -DELF_BUILD $< -o $@

$(STAGE2_ELF): $(STAGE2_OBJ) | $(BUILD_BOOT)
	$(ECHO) "  LD      $@"
	$(Q)$(LD) -m elf_i386 -Ttext $(STAGE2_LOAD_ADDR) --oformat=elf32-i386 $< -o $@

# --- Directory creation ---
$(BUILD_BOOT):
	$(Q)mkdir -p $@

clean-bootloader:
	$(Q)rm -rf $(BUILD_BOOT)