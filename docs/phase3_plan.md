Phase 3: Memory Management
---

### 🔧 Week 1: Memory Map & BIOS Interface (e820 Parser)

**Goals:**

* Parse BIOS-provided e820 memory map from multiboot-compliant bootloader.
* Mark reserved, available, ACPI, and unusable regions.

**Tasks:**

* Add bootloader-to-kernel interface to pass e820 map.
* Parse entries and display memory regions with start, end, and type.
* Store results in a global memory descriptor array.

**Deliverables:**

* `memmap.c/.h` with a clean memory region parser.
* Kernel boot prints parsed map on screen.

```
==================================================
Parsing BIOS Memory Map (E820)...

[MEMORY MAP] BIOS provided 6 entries:
[0] Base: 0x0000000000000000, Length: 0x000000000009fc00, Type: Available
[1] Base: 0x000000000009fc00, Length: 0x0000000000000400, Type: Reserved
[2] Base: 0x00000000000f0000, Length: 0x0000000000010000, Type: Reserved
[3] Base: 0x0000000000100000, Length: 0x0000000007ee0000, Type: Available
[4] Base: 0x0000000007fe0000, Length: 0x0000000000020000, Type: Reserved
[5] Base: 0x00000000fffc0000, Length: 0x0000000000040000, Type: Reserved

[MEMORY MAP] Usable memory regions count: 2

==================================================
```

---

### 🔨 Week 2: Physical Memory Frame Allocator (Bitmap)

**Goals:**

* Implement a bitmap-based frame allocator (e.g., 4KiB pages).
* Skip reserved/used regions from e820 map.

**Tasks:**

* Allocate memory for bitmap (1 bit per frame).
* Functions: `pmm_alloc_frame()`, `pmm_free_frame()`.
* Reserve kernel image, VGA buffer, etc., at boot.

**Deliverables:**

* `pmm.c/.h` with alloc/free logic.
* Unit test kernel mode bitmap allocator and validate frame reuse.

---

### 🧠 Week 3: Basic Paging Setup

**Goals:**

* Enable paging with identity-mapped kernel and boot structures.
* Create initial page directory/table manually.

**Tasks:**

* Setup page directory and tables in C and Assembly.
* Enable paging by setting CR3 and CR0.PG bit.
* Map VGA, kernel, heap, etc.

**Deliverables:**

* `paging.c/.h` with `init_paging()` function.
* Confirm all addresses work after paging is enabled.
* Add page fault handler stub.

---

### 📦 Week 4: Kernel Heap + Allocator

**Goals:**

* Build `kmalloc`/`kfree` using bump or stack allocator.
* Support page-aligned allocations.

**Tasks:**

* Define kernel heap start and end.
* Implement simple allocator for dynamic memory.
* Later: hook into `pmm_alloc_frame()` for expansion.

**Deliverables:**

* `heap.c/.h` with `kmalloc(size)` and `kfree()`.
* Track allocated blocks for debugging (optionally add metadata).

---

### 🚀 Week 5: Custom Enhancements for Innovation

**Goals:**

* Add unique, differentiating memory features for your kernel.

**Ideas:**

* Real-time graphical memory map visualizer.
* Per-frame usage counters for performance monitoring.
* Lazy page fault logging system for education/debug.
* Memory region tagging (e.g., DMA-safe, shared, device-mapped).
* Memory corruption detection or guard pages.