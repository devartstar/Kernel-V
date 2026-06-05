# ==================================================================
# Project Configuration
# ==================================================================

# --- Cross-compilation toolchain ---
TARGET 			:= i686-elf
CROSS_PREFIX 	?= $(TARGET)-

# --- Compilers ---
HOSTCC 			:= gcc
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
# m32					: Forces 32 bit code.
# nostdlib				: Removes all standard library stuff.
# fno-builtin			: Prevents compiler magic for optimization.
# fno-stack-protector	: Disable stack smashing protection
# ffreestanding			: Tell compiler we are writing an OS/boot environment.
ARCH_FLAGS 		:= -m32
COMMON_FLAGS 	:= -ffreestanding -nostdlib -fno-builtin -fno-stack-protector

# --- Version Flags ---
VERSION_FLAGS := -DKERNEL_VERSION=\"$(VERSION)\" -DBUILD_DATE=\"$(BUILD_DATE)\"

# --- C Compiler Flags ---
CFLAGS_BASE 	:= $(ARCH_FLAGS) $(COMMON_FLAGS) -std=c99 -Wall -Wextra $(VERSION_FLAGS)
CFLAGS_OPT 		:= -O2 -fomit-frame-pointer
CFLAGS_DEBUG 	:= -g -O0 -DDEBUG
CFLAGS_TEST := -DKERNEL_TESTS=$(CONFIG_BUILD_TEST)
ifeq ($(CONFIG_TESTS_UNIT),y)
	CFLAGS_TEST += -DUNIT_TESTS=1
endif
ifeq ($(CONFIG_TESTS_INTEGRATION),y)
	CFLAGS_TEST += -DINTEGRATION_TEST=1
endif

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
ifeq ($(CONFIG_BUILD_DEBUG)$(CONFIG_BUILD_TEST),yy)
	# Debug + Test: include both debug and test flags
	CFLAGS 		:= $(CFLAGS_BASE) $(CFLAGS_DEBUG) $(CFLAGS_TEST) $(INCLUDE_PATHS)
	CXXFLAGS 	:= $(CXXFLAGS_BASE) $(CXXFLAGS_DEBUG) $(INCLUDE_PATHS)
else ifeq ($(CONFIG_BUILD_DEBUG), y)
	# Debug only
	CFLAGS 		:= $(CFLAGS_BASE) $(CFLAGS_DEBUG) $(INCLUDE_PATHS)
	CXXFLAGS 	:= $(CXXFLAGS_BASE) $(CXXFLAGS_DEBUG) $(INCLUDE_PATHS)
else
	# Release
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
