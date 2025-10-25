# ==============================================================================
# Kernel-V Advanced Build System  
# ==============================================================================

# --- Project Configuration ---
PROJECT_NAME := Kernel-V
VERSION := 0.5.0
BUILD_DATE := $(shell date +%Y-%m-%d)

# --- Build Configuration ---
BUILD_TYPE ?= release  # release, debug, test
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
ifeq ($(BUILD_TYPE),debug)
	include $(MAKE_DIR)/debug.mk
else ifeq ($(BUILD_TYPE),test)
	include $(MAKE_DIR)/test.mk
else
	# Include debug.mk even for release to get debug targets
	include $(MAKE_DIR)/debug.mk
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
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-20s %s\n", $$1, $$2}'
	@echo ""
	@echo "Examples:"
	@echo "  make clean all             Clean and build"
	@echo "  make BUILD_TYPE=debug run  Build and run in debug mode"
	@echo "  make test-unit VERBOSE=1   Run unit tests with verbose output"

.PHONY: help
