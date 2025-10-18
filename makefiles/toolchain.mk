# ==================================================================
# Project Configuration
# ==================================================================

# --- Cross-compilation toolchain ---
TARGET 			:= i686-elf
CROSS_PREFIX 	?= $(TARGET)-

# --- Compilers ---
CC 				:= $(CROSS_PREFIX)gcc
CXX 			:= $(CROSS_PREFIX)g++
LD 				:= $(CROSS_PREFIX)ld
AS 				:= $(CROSS_PREFIX)ar
OBJCOPY 		:= $(CROSS_PREFIX)objcopy
OBJDUMP 		:= $(CROSS_PREFIX)objdump
NM 				:= $(CROSS_PREFIX)nm
STRIP 			:= $(CROSS_PREFIX)strip

# --- Assembler ---
NASM 			:= nasm
NASM_FORMAT 	:= elf32

# --- Common Flags ---
ARCH_FLAGS 		:= -m32
COMMON_FLAGS 	:= -ffreestanding -nostdlib -fno-builtin -fno-stack-protector

# --- C Compiler Flags ---
CFLAGS_BASE 	:= $(ARCH_FLAGS) $(COMMON_FLAGS) -std=c99 -Wall -Wextra
CFLAGS_OPT 		:= -O2 -fomit-frame-pointer
CFLAGS_DEBUG 	:= -g -O0 -DDEBUG
CFLAGS_TEST 	:= -DKERNEL_TESTS=$(ENABLE_TESTS)

# --- C++ Compiler Flags ---
CXXFLAGS_BASE 	:= $(ARCH_FLAGS) $(COMMON_FLAGS) -std=c++11 -fno-exceptions -fno-rtti
CXXFLAGS_OPT 	:= -O2 -fomit-frame-pointer
CXXFLAGS_DEBUG 	:= -g -O0 -DDEBUG

# --- Assembler Flags ---
ASFLAGS 		:= -F stabs
NASMFLAGS 		:= -f $(NASM_FORMAT) -g

# --- Linker Flags ---
LDFLAGS 		:= -m elf_i386 -nostdlib

# --- Tools ---
QEMU 			:= qemu-system-i386
GDB 			:= gdb
HEXDUMP			:= hexdump
DD 				:= dd

# --- Build type specific flags ---
ifeq ($(BUILD_TYPE), debug)
	CFLAGS 		:= $(CFLAGS_BASE) $(CFLAGS_DEBUG) $(INCLUDE_PATHS)
	CXXFLAGS 	:= $(CXXFLAGS_BASE) $(CXXFLAGS_DEBUG) $(INCLUDE_PATHS)
else ifeq ($(BUILD_TYPE), test)
	CFLAGS		:= $(CFLAGS_BASE) $(CFLAGS_DEBUG) $(CFLAGS_TEST) $(INCLUDE_PATHS)
	CXXFLAGS	:= $(CXXFLAGS_BASE) $(CXXFLAGS_DEBUG) $(INCLUDE_PATHS)
else
	CFLAGS 		:= $(CFLAGS_BASE) $(CFLAGS_OPT) $(INCLUDE_PATHS)
	CXXFLAGS 	:= $(CXXFLAGS_BASE) $(CXXFLAGS_OPT) $(INCLUDE_PATHS)
endif

# --- Verbose Output ---
ifeq ($(VERBOSE), 1)
	Q :=
	ECHO := @echo
else
	Q := @
	ECHO := @echo
endif
