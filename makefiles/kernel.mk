# ==============================================================================
# Kernel Build  
# ==============================================================================

# --- Kernel Linker Script ---
KERNEL_LD := $(KERNDIR)/linker/kernel.ld

# --- Kernel Entry Point ---
KERNEL_ENTRY_SRC := $(KERN_ARCH_DIR)/boot/kernel_entry.asm
KERNEL_ENTRY_OBJ := $(BUILD_KERN)/kernel_entry.o

# --- Auto-generated Headers ---
PROC_OFFSET_GEN := $(KERNDIR)/lib/proc_offset_generator
PROC_OFFSET_HDR := $(INCDIR)/proc/proc_offset_asm.h

# --- Source File Discovery ---
KERNEL_C_SOURCES := $(shell find $(KERNDIR) -name "*.c" -not -path "$(TESTDIR)/*" -not -name "*_generator.c")
KERNEL_ASM_SOURCES := $(shell find $(KERNDIR) -name "*.asm" -not -path "$(BOOTDIR)/*" -not -name "kernel_entry.asm" -not -path "$(KERN_ARCH_DIR)/user/userprog*.asm")

# --- Object File Generation ---
KERNEL_C_OBJECTS := $(patsubst $(KERNDIR)/%.c,$(BUILD_KERN)/%.o,$(KERNEL_C_SOURCES))
KERNEL_ASM_OBJECTS := $(patsubst $(KERNDIR)/%.asm,$(BUILD_KERN)/%.o,$(KERNEL_ASM_SOURCES))

# --- User Program for Integration Tests (when needed) ---
ifeq ($(CONFIG_TESTS_INTEGRATION), y)
    USERPROG_ASM_MAIN := $(KERN_ARCH_DIR)/user/userprog.asm
    USERPROG_BIN_MAIN := $(BUILD_KERN)/userprog.bin
    USERPROG_OBJ_MAIN := $(BUILD_KERN)/userprog.o

    USERPROG_A_ASM := $(KERN_ARCH_DIR)/user/userprog_a.asm
    USERPROG_A_BIN := $(BUILD_KERN)/userprog_a.bin
    USERPROG_A_OBJ := $(BUILD_KERN)/userprog_a.o

    USERPROG_B_ASM := $(KERN_ARCH_DIR)/user/userprog_b.asm
    USERPROG_B_BIN := $(BUILD_KERN)/userprog_b.bin
    USERPROG_B_OBJ := $(BUILD_KERN)/userprog_b.o
endif

# --- Test Sources (conditional) ---
ifeq ($(CONFIG_BUILD_TEST), y)
    TEST_C_SOURCES := $(shell find $(TESTDIR) -name "*.c")
    TEST_C_OBJECTS := $(patsubst $(KERNDIR)/%.c,$(BUILD_KERN)/%.o,$(TEST_C_SOURCES))
    ifeq ($(CONFIG_TESTS_INTEGRATION), y)
        KERNEL_OBJECTS := $(KERNEL_ENTRY_OBJ) $(KERNEL_C_OBJECTS) $(KERNEL_ASM_OBJECTS) $(TEST_C_OBJECTS) $(USERPROG_OBJ_MAIN) $(USERPROG_A_OBJ) $(USERPROG_B_OBJ)
    else
        KERNEL_OBJECTS := $(KERNEL_ENTRY_OBJ) $(KERNEL_C_OBJECTS) $(KERNEL_ASM_OBJECTS) $(TEST_C_OBJECTS)
    endif
else
    KERNEL_OBJECTS := $(KERNEL_ENTRY_OBJ) $(KERNEL_C_OBJECTS) $(KERNEL_ASM_OBJECTS)
endif

# --- Output Files ---
KERNEL_ELF := $(BUILD_KERN)/kernel.elf
KERNEL_BIN := $(BUILD_KERN)/kernel.bin
KERNEL_MAP := $(BUILD_KERN)/kernel.map
KERNEL_SYM := $(BUILD_KERN)/kernel.sym

.PHONY: kernel clean-kernel kernel-info

kernel: $(KERNEL_BIN) $(KERNEL_ELF) $(KERNEL_SYM)

# --- Auto-generate struct offsets ---
$(PROC_OFFSET_HDR): $(PROC_OFFSET_GEN)
	$(ECHO) "  GEN     $@"
	$(Q)$< > $@

$(PROC_OFFSET_GEN): $(KERNDIR)/lib/data_structure/proc_offset_generator.c
	$(ECHO) "  HOSTCC  $@"
	$(Q)gcc -I$(INCDIR) -o $@ $<

# --- Kernel Entry Point ---
$(KERNEL_ENTRY_OBJ): $(KERNEL_ENTRY_SRC) $(PROC_OFFSET_HDR) | $(BUILD_KERN)
	$(ECHO) "  ASM     $@"
	$(Q)$(NASM) $(NASMFLAGS) $< -o $@

# --- Pattern Rules ---
$(BUILD_KERN)/%.o: $(KERNDIR)/%.c $(INCDIR)/kconfig.h | $(BUILD_KERN)
	$(ECHO) "  CC      $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_KERN)/%.o: $(KERNDIR)/%.asm $(PROC_OFFSET_HDR) | $(BUILD_KERN)
	$(ECHO) "  ASM     $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(NASM) $(NASMFLAGS) $< -o $@

# --- User Program Build Rules ---
ifeq ($(CONFIG_TESTS_INTEGRATION), y)
$(USERPROG_BIN_MAIN): $(USERPROG_ASM_MAIN) | $(BUILD_KERN)
	$(ECHO) "  ASM-USER $@"
	$(Q)$(NASM) -f bin $< -o $@

$(USERPROG_OBJ_MAIN): $(USERPROG_BIN_MAIN) | $(BUILD_KERN)
	$(ECHO) "  OBJCOPY $@"
	$(Q)(cd $(BUILD_KERN) && \
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 \
		--rename-section .data=.rodata,contents,alloc,load,readonly,data \
		--redefine-sym _binary_userprog_bin_start=_binary_userprog_start \
		--redefine-sym _binary_userprog_bin_end=_binary_userprog_end \
		userprog.bin userprog.o)

$(USERPROG_A_BIN): $(USERPROG_A_ASM) | $(BUILD_KERN)
	$(ECHO) "  ASM-USER $@"
	$(Q)$(NASM) -f bin $< -o $@

$(USERPROG_A_OBJ): $(USERPROG_A_BIN) | $(BUILD_KERN)
	$(ECHO) "  OBJCOPY $@"
	$(Q)(cd $(BUILD_KERN) && \
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 \
		--rename-section .data=.rodata,contents,alloc,load,readonly,data \
		--redefine-sym _binary_userprog_a_bin_start=_binary_userprog_a_start \
		--redefine-sym _binary_userprog_a_bin_end=_binary_userprog_a_end \
		userprog_a.bin userprog_a.o)

$(USERPROG_B_BIN): $(USERPROG_B_ASM) | $(BUILD_KERN)
	$(ECHO) "  ASM-USER $@"
	$(Q)$(NASM) -f bin $< -o $@

$(USERPROG_B_OBJ): $(USERPROG_B_BIN) | $(BUILD_KERN)
	$(ECHO) "  OBJCOPY $@"
	$(Q)(cd $(BUILD_KERN) && \
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 \
		--rename-section .data=.rodata,contents,alloc,load,readonly,data \
		--redefine-sym _binary_userprog_b_bin_start=_binary_userprog_b_start \
		--redefine-sym _binary_userprog_b_bin_end=_binary_userprog_b_end \
		userprog_b.bin userprog_b.o)
endif

# --- Kernel Linking ---
$(KERNEL_ELF): $(KERNEL_OBJECTS) $(KERNEL_LD) | $(BUILD_KERN)
	$(ECHO) "  LD      $@"
	$(Q)$(LD) $(LDFLAGS) -T $(KERNEL_LD) -o $@ $(KERNEL_OBJECTS) -Map $(KERNEL_MAP)

$(KERNEL_BIN): $(KERNEL_ELF) | $(BUILD_KERN)
	$(ECHO) "  OBJCOPY $@"
	$(Q)$(OBJCOPY) -O binary $< $@

# --- Symbol Table ---
$(KERNEL_SYM): $(KERNEL_ELF) | $(BUILD_KERN)
	$(ECHO) "  NM      $@"
	$(Q)$(NM) $< > $@

# --- Directory Creation ---
$(BUILD_KERN):
	$(Q)mkdir -p $@
	$(Q)mkdir -p $(BUILD_KERN)/arch/x86/{boot,cpu,interrupt,memory}
	$(Q)mkdir -p $(BUILD_KERN)/core/{debug,init,panik}
	$(Q)mkdir -p $(BUILD_KERN)/drivers/video
	$(Q)mkdir -p $(BUILD_KERN)/lib/{printf,string,data_structure}
	$(Q)mkdir -p $(BUILD_KERN)/mm/{physical,virtual,allocator}
	$(Q)mkdir -p $(BUILD_KERN)/proc/{context,scheduler}
	$(Q)mkdir -p $(BUILD_KERN)/time
	$(Q)mkdir -p $(BUILD_KERN)/tests/{unit,proc}

kernel-info: $(KERNEL_ELF) ## Show kernel information
	$(ECHO) "Kernel Information:"
	$(ECHO) "  Size: $$(stat -c%s $(KERNEL_BIN)) bytes"
	$(ECHO) "  Entry point: $$($(OBJDUMP) -f $(KERNEL_ELF) | grep 'start address' | cut -d' ' -f3)"
	$(ECHO) "  Sections:"
	$(Q)$(OBJDUMP) -h $(KERNEL_ELF) | grep -E '^\s+[0-9]+'

clean-kernel: ## Clean kernel build artifacts
	$(Q)rm -rf $(BUILD_KERN)
	$(Q)rm -f $(PROC_OFFSET_HDR) $(PROC_OFFSET_GEN)
