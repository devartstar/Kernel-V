# ==============================================================================
# Kernel-V Advanced Build System  
# ==============================================================================
-include .config

# --- Project Configuration ---
PROJECT_NAME := Kernel-V
VERSION := 0.7.1
BUILD_DATE := $(shell date +%Y-%m-%d)

# Default QEMU settings
QEMU_DRIVE_FLAGS ?= -drive format=raw,file=
QEMU_DISPLAY ?= -display curses
QEMU_SERIAL ?= -serial file:serial.log
QEMU_EXTRA ?=

# Machine specific overrrides
-include local.mk

# --- Build Configuration ---
BUILD_TYPE ?= unknown
VERBOSE ?= 0

# --- Directory Structure ---
ROOT_DIR := $(CURDIR)
MAKE_DIR := $(ROOT_DIR)/makefiles
TOOLS_DIR := $(ROOT_DIR)/tools

# --- Include Common Configuration ---
include $(MAKE_DIR)/config.mk
include $(MAKE_DIR)/toolchain.mk
include $(MAKE_DIR)/kconfig.mk

# --- Component Includes ---
include $(MAKE_DIR)/bootloader.mk
include $(MAKE_DIR)/kernel.mk
include $(MAKE_DIR)/targets.mk

# --- Conditional Includes Based on Build Type ---
ifeq ($(CONFIG_BUILD_DEBUG)$(CONFIG_BUILD_TEST),yy)
# Case 1: Debug + Test
	include $(MAKE_DIR)/debug.mk
	include $(MAKE_DIR)/test.mk	
	BUILD_TYPE := test
else ifeq ($(CONFIG_BUILD_DEBUG),y)
# Case 2: Debug only (no test)
	include $(MAKE_DIR)/debug.mk
	BUILD_TYPE := debug
else
# Case 3: Release (production)
	BUILD_TYPE := release
endif

# --- Default Target ---
.DEFAULT_GOAL := help

# --- Local Configs ---
qemu: ## Show local qemu configuration
	@echo "Qemu Flags: $(QEMU_DRIVE_FLAGS)"
	@echo "Qemu Display: $(QEMU_DISPLAY)"
	@echo "Qemu Serial: $(QEMU_SERIAL)"
	@echo "Qemu Extra: $(QEMU_EXTRA)"

# --- Help System ---
help: ## Show this help message
	@echo "$(PROJECT_NAME) v$(VERSION) - Advanced Build System"
	@echo "=================================================="
	@echo ""
	@echo "Build Types:"
	@echo "  make BUILD_TYPE=release    Build production kernel (default)"
	@echo "  make BUILD_TYPE=debug      Build with debug symbols and features"
	@echo "  make BUILD_TYPE=test       Build with full test suite"
	@echo ""
	@echo "Available targets:"
		@awk '\
	  BEGIN { FS=":.*##[ \t]*" } \
	  /^[[:alnum:]_.-]+:.*##[ \t]*/ { \
		n = split(FILENAME, p, "/"); f = p[n]; \
		match($$0, /^[[:alnum:]_.-]+/); t = substr($$0, RSTART, RLENGTH); \
		items[f] = items[f] "  - " t "  # " $$2 "\n"; \
	  } \
	  END { \
		n = split("$(MAKEFILE_LIST)", fl, /[ \t]+/); \
		for (i=1; i<=n; i++) { \
		  m = split(fl[i], q, "/"); f = q[m]; \
		  if (items[f] != "") printf "%s\n%s", f, items[f]; \
		} \
	  }' $(MAKEFILE_LIST)
	@echo ""
	@echo "Examples:"
	@echo "  make clean all             Clean and build"
	@echo "  make BUILD_TYPE=debug run  Build and run in debug mode"
	@echo "  make test-unit VERBOSE=1   Run unit tests with verbose output"

.PHONY: help qemu

# make menuconfig
# BUILD RELEASE:
# 	make all
# 	make run
# 	make clean all
#
# BUILD TESTS:
# 	make test-unit
# 	make test-integration
# 	make test-all
#
# BUILD DEBUG:
# 	make all
# 	make debug
# 	make debug-kernel
# 	make debug-bootloader
# 	make verify-symbols
# 	make run-debug

