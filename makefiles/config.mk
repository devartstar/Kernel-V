# ==================================================================
# Project Configuration
# ==================================================================

# --- Directories ---
BOOTDIR	 := bootloader
KERNDIR  := kernel
BUILDDIR := build
INCDIR	 := $(KERNDIR)/include
TESTDIR  := $(KERNDIR)/tests
DOCSDIR  := docs

# --- Build Output Directories ---
BUILD_BOOT := $(BUILDDIR)/bootloader
BUILD_KERN := $(BUILDDIR)/kernel
BUILD_TEST := $(BUILDDIR)/tests
BUILD_DOCS := $(BUILDDIR)/docs

# --- Source Paths ---
KERN_ARCH_DIR 		:= $(KERNDIR)/arch/x86
KERNEL_CORE_DIR 	:= $(KERNDIR)/core
KERNEL_DRIVERS_DIR 	:= $(KERNDIR)/drivers
KERN_LIB_DIR 		:= $(KERNDIR)/lib
KERN_MM_DIR 		:= $(KERNDIR)/mm
KERNEL_PROC_DIR 	:= $(KERNDIR)/proc
KERNEL_TIME_DIR 	:= $(KERNDIR)/time

# --- Include Paths ---
INCLUDE_PATHS := \
	-I$(INCDIR) \
	-I$(INCDIR)/arch/x86 \
	-I$(INCDIR)/core \
	-I$(INCDIR)/drivers \
	-I$(INCDIR)/lib \
	-I$(INCDIR)/mm \
	-I$(INCDIR)/proc \
	-I$(INCDIR)/time \
	-I$(INCDIR)/tests

# --- Disk Layout ---
STAGE1_SECTOR := 0
STAGE2_SECTOR := 1
KERNEL_SECTOR := 9

# --- Memory Layout ---
KERNEL_LOAD_ADDR := 0x100000
STAGE1_LOAD_ADDR := 0x7C00
STAGE2_LOAD_ADDR := 0x7E00

# --- Feature Flags ---
TESTS_ENABLED ?= 0
DEBUG_ENABLED ?= 0
