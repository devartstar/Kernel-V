# Kernel-V Phase 3: Memory Management

## Overview

This phase builds core memory management systems for the kernel, including physical memory tracking, paging support, and dynamic heap allocation. The system is designed to be modular, extensible, and production-quality.

---

## 1. Memory Map Parsing (e820 Interface)

**Objective:** Parse the memory map provided by the BIOS using the e820 interface to identify usable and reserved regions.

**Features:**

* Interface with bootloader to receive e820 map
* Parse and store memory regions: base address, length, type
* Display parsed memory regions at boot
* Classify regions as:

  * Available
  * Reserved
  * ACPI
  * Unusable

**Implementation:**

* `src/memory/memmap.c` and `memmap.h`
* Exported memory descriptor table to kernel
* Logs on-screen with type decoding

---

## 2. Physical Page Frame Allocator (Bitmap-Based)

**Objective:** Implement a physical frame allocator using a bitmap, where each bit represents one 4KiB frame.

**Features:**

* Allocates and frees physical frames
* Marks reserved regions as unavailable
* Lazy bitmap initialization based on e820

**API:**

* `void* pmm_alloc_frame()`
* `void  pmm_free_frame(void*)`

**Implementation:**

* `src/memory/pmm.c` and `pmm.h`
* Debug logs and assertions for safety
* Integration with memory map for usable regions

---

## 3. Paging and Virtual Memory Initialization

**Objective:** Enable x86 paging to provide virtual memory abstraction.

**Features:**

* Identity map kernel regions
* Set up page directory and page tables
* Enable MMU by writing to CR3 and CR0
* Add stub for page fault handler

**Implementation:**

* `src/memory/paging.c` and `paging.h`
* `init_paging()` to construct mappings
* VGA, kernel code/data, heap are mapped
* Page fault ISR for diagnostics

---

## 4. Kernel Heap and Dynamic Allocator

**Objective:** Provide `kmalloc`/`kfree` for dynamic memory use in kernel.

**Features:**

* Simple bump or stack allocator
* Aligns allocations to word/page boundaries
* Tracks allocated blocks (optionally with metadata)
* Configurable heap region (start and max end)

**API:**

* `void* kmalloc(size_t size)`
* `void  kfree(void* ptr)`

**Implementation:**

* `src/memory/heap.c` and `heap.h`
* Allocator grows using physical frame allocator
* Optional debug features (block sizes, tags)

---

## 5. Innovation and Custom Enhancements

**Objective:** Add unique, differentiating features to make the kernel memory system more usable, educational, or powerful.

**Ideas:**

* **Memory Visualizer:** Real-time VGA visualization of memory regions
* **Reference Counters:** Track frame usage over time
* **Fault Logger:** Capture stack trace on page faults
* **Region Tagging:** Label memory blocks (DMA-safe, device-owned, etc.)
* **Guard Pages:** Use unmapped guard pages to detect overflows

**Implementation:**

* Add feature toggles in config headers
* Modularize enhancements into separate files (`memviz.c`, `guard.c`, etc.)
* Document rationale and benefit of each feature

---

## References

* **The Design of the UNIX Operating System – Maurice Bach:** Ch. 2.2, 9
* **Advanced Programming in the UNIX Environment (APUE):** Ch. 7.8, 7.11

---

## Developer Notes

* Place memory modules in `src/memory/`
* Shared kernel state and interfaces go in `include/kernel/`
* Enable debug macros to trace memory flow
* Write tests in the boot/early init phase to validate allocators