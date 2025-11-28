# ==============================================================================
# Kernel-V Advanced Build System  
# ==============================================================================
-include .config

# --- Project Configuration ---
PROJECT_NAME := Kernel-V
VERSION := 0.5.0
BUILD_DATE := $(shell date +%Y-%m-%d)

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

# --- Conditional Includes Based on Build Type ---
ifeq ($(CONFIG_BUILD_DEBUG),y)
	include $(MAKE_DIR)/debug.mk
	BUILD_TYPE := debug
else ifeq ($(CONFIG_BUILD_TEST),y)
	include $(MAKE_DIR)/test.mk
	BUILD_TYPE := test
else
	# CONFIG_BUILD_RELEASE=y
	# Include debug.mk even for release to get debug targets
	include $(MAKE_DIR)/debug.mk
	BUILD_TYPE := release
endif

# --- Component Includes ---
include $(MAKE_DIR)/bootloader.mk
include $(MAKE_DIR)/kernel.mk
include $(MAKE_DIR)/test.mk
include $(MAKE_DIR)/targets.mk

# --- Default Target ---
.DEFAULT_GOAL := help

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

.PHONY: help

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

