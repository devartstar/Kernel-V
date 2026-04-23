# Phase 1 — Foundation Hardening for Device Work

## Phase 1 Goal
Prepare Kernel-V so it can safely support PCI/device drivers without leaking low-level memory, MMIO, and interrupt assumptions into every driver.

This phase is complete when the kernel can:
- allocate page-aligned contiguous memory for device-facing buffers
- expose both virtual and physical addresses for such allocations
- cleanly separate MMIO from port I/O
- safely protect shared driver/device state with minimal locking
- expose a minimal device-facing interrupt registration abstraction

---

# Subphase 1.1 — Addressing Model and Memory Contract Audit

## Purpose
Before changing allocators or adding DMA helpers, establish the **exact addressing contract** of the current kernel.

We must answer:
- what is a kernel virtual address in your kernel?
- when does `virt == phys` hold, and when does it not?
- what memory ranges are identity mapped?
- what memory ranges are higher-half or remapped?
- which kernel allocators currently return virtual addresses only?
- how does the kernel currently recover a physical address from a kernel pointer?

## Why this subphase comes first
If you do not have a precise memory contract, every later abstraction will be built on guesses.

PCI/device work is unforgiving here:
- the CPU dereferences **virtual addresses**
- devices use **physical/bus-visible addresses**
- drivers often need **both**
- confusion between them causes silent corruption

## Main learning goals
You should understand:
- the difference between virtual address, physical address, and device-visible address
- why a kernel pointer is not automatically safe to hand to hardware
- where your current kernel’s page tables make translation obvious vs non-obvious
- how your PMM and paging layers currently relate

## Technical tasks
- inspect current kernel memory layout
- inspect kernel page mapping policy
- inspect PMM allocation return type and invariants
- identify all places assuming `virt == phys`
- define explicit address types:
  - `phys_addr_t`
  - `virt_addr_t`
- write down the kernel memory contract in comments/doc form

## Deliverable
A written and code-level understanding of:
- what an allocated kernel pointer means
- how to derive its physical address
- where that is currently safe and where it is not

## Exit criteria
You can answer, with certainty:
- given a kernel pointer returned by allocator X, how do I get its physical address?
- is that always valid?
- under what mapping assumptions?

# Subphase 1.2 — Physical Page Allocator Hardening

## Purpose
Strengthen the PMM so it can support device-oriented allocations instead of only general kernel use.

## Why this comes second
Once the address model is clear, the next thing to harden is the allocator that owns physical frames.

You cannot build DMA-style helpers on top of a PMM that:
- cannot allocate multiple contiguous pages
- cannot free them reliably
- does not expose physical identity clearly

## Main learning goals
You should understand:
- what a frame allocator really manages
- why “usable memory count” is not enough — you need frame-accurate bookkeeping
- the difference between:
  - single-page allocation
  - multi-page contiguous allocation
  - arbitrary-page allocation
- why fragmentation matters for device-facing memory

## Technical tasks
- review PMM frame bitmap / data structure correctness
- ensure physical frame numbering is explicit and reversible
- add:
  - allocate one page
  - allocate N contiguous pages
  - free N contiguous pages
- ensure alignment invariants are explicit
- add physical range validation
- add debugging checks / assertions for double free or out-of-range free

## Deliverable
A PMM that can safely say:
- I allocated N contiguous physical pages starting at P
- I can later free exactly that same run

## Exit criteria
You can:
- allocate 1 page
- allocate multiple contiguous pages
- free them correctly
- trust the returned physical base

# Subphase 1.3 — Kernel Virtual Mapping for Allocated Physical Pages

## Purpose
Bridge physical page allocation to usable kernel memory.

The PMM gives you physical frames.
Drivers need kernel-accessible virtual addresses too.

So this subphase defines how allocated physical pages become usable in the kernel address space.

## Why this comes here
Subphase 1.2 gives us trustworthy physical frames.
Now we need a clean path from:
- physical allocation
to
- kernel virtual usability

without smearing paging internals into future drivers.

## Main learning goals
You should understand:
- why PMM allocation alone is not enough
- how physical pages become CPU-accessible through paging
- why drivers should not manipulate page tables directly
- the difference between:
  - pre-existing direct mapping / identity mapping
  - explicit kernel mapping of allocated pages

## Technical tasks
- inspect current kernel heap / page allocation mapping path
- determine whether new physical pages are already reachable virtually
- if not, add a helper path to map allocated pages into kernel VA
- define the allocator contract:
  - returned KVA
  - corresponding physical base
  - page count
- keep paging details hidden from higher layers

## Deliverable
A kernel-internal mechanism that can produce:
- a usable kernel virtual pointer
- a matching physical address
- for N contiguous pages

## Exit criteria
Given an allocation request, the kernel can hand back:
- `void* kva`
- `phys_addr_t phys`
- `count`
and both are known-correct

# Subphase 1.4 — DMA-Style Allocation Abstraction

## Purpose
Build the first hardware-facing memory abstraction.

This does not need to be a full DMA subsystem yet.
It just needs to create a **device-safe allocation interface** that hides PMM + paging details.

## Why this comes after allocator hardening
Because this abstraction should sit on top of correct lower layers, not replace them.

## Main learning goals
You should understand:
- why drivers should never call raw PMM functions directly
- how a DMA allocation API becomes the hardware contract boundary
- why even a simple early implementation is worth it

## Technical tasks
Design and implement an API such as:
- `void* dma_alloc_pages(size_t count, phys_addr_t* out_phys);`
- `void dma_free_pages(void* kva, size_t count);`

Possible extension:
- a small allocation descriptor structure containing:
  - virtual base
  - physical base
  - count
  - size in bytes

Also define exact semantics:
- guaranteed page alignment?
- guaranteed contiguity?
- zero-filled or not?
- usable in interrupt context or not?
- ownership / lifetime rules

## Deliverable
A clean device-memory API that future drivers can use without knowing PMM/paging internals.

## Exit criteria
A driver author can request device-facing memory and immediately obtain:
- CPU-usable virtual pointer
- hardware-usable physical address

# Subphase 1.5 — MMIO Access Layer

## Purpose
Introduce a strict and explicit interface for memory-mapped device register access.

## Why this comes before locks and IRQ abstraction
Because once we start touching real devices, register access semantics must already be explicit.

## Main learning goals
You should understand:
- why MMIO is not “normal RAM”
- why device registers need typed volatile access
- why width matters:
  - 8-bit
  - 16-bit
  - 32-bit
- why port I/O and MMIO must stay conceptually separate

## Technical tasks
Implement:
- `mmio_read8`
- `mmio_read16`
- `mmio_read32`
- `mmio_write8`
- `mmio_write16`
- `mmio_write32`

Possibly later:
- 64-bit variants if needed

Also define:
- parameter types
- use of `volatile`
- whether barriers are needed yet
- conventions for register offsets

Add clear separation in codebase between:
- `inb/outb/...` style port I/O
- MMIO helper usage

## Deliverable
A minimal MMIO library that future device drivers must use.

## Exit criteria
No driver-facing code should need to cast random pointers to volatile integer pointers manually.

# Subphase 1.6 — Minimal Spinlock and IRQ-Safe Locking Layer

## Purpose
Protect shared device/driver state from concurrency bugs.

Even on a single CPU kernel, interrupts create concurrency:
- mainline code may modify queue state
- interrupt handler may modify completion state
- without locking, these race

## Why this comes after MMIO
First we create safe register access.
Then we create safe shared-state access around device operations.

## Main learning goals
You should understand:
- why interrupts create concurrency even before SMP
- why plain `cli/sti` is not a substitute for a lock API
- what a spinlock means in a uniprocessor + interrupt model
- why `irqsave/irqrestore` variants matter

## Technical tasks
Implement:
- `spinlock_t`
- `spin_lock`
- `spin_unlock`
- `spin_lock_irqsave`
- `spin_unlock_irqrestore`

Define rules:
- what state can be protected by plain lock
- what state must use irqsave
- whether recursive locking is forbidden
- debug assertions for misuse

## Deliverable
A minimal locking API suitable for:
- queue state
- request state
- ISR/mainline coordination

## Exit criteria
You have a clear rule for how future driver state is protected against interrupt races.

# Subphase 1.7 — Device-Facing Interrupt Registration Abstraction

## Purpose
Create the smallest useful abstraction between hardware IRQ plumbing and device drivers.

## Why this is last in Phase 1
Because interrupts sit at the top of the hardware-facing foundation:
- memory contract must exist
- MMIO access must exist
- lock discipline must exist

Only then should we define how devices receive interrupts.

## Main learning goals
You should understand:
- why drivers should not be tightly bound to raw IDT/PIC details
- how a device-facing IRQ registration interface decouples bus/device code from interrupt plumbing
- why this abstraction matters now, even if the backend is still legacy PIC/INTx

## Technical tasks
Add a minimal API such as:
- register device IRQ handler
- unregister handler
- dispatch handler with driver-owned context pointer

Define:
- handler signature
- return semantics
- acknowledgment responsibility
- shared IRQ assumptions or not
- where PIC EOI happens

Keep it simple for now:
- no MSI/MSI-X yet
- no APIC dependency yet
- just enough for PCI INTx-backed drivers later

## Deliverable
A future PCI driver can say:
“bind this IRQ line to this handler and my device context”
without knowing the low-level interrupt plumbing.

## Exit criteria
There is now a device-oriented IRQ hook layer between generic drivers and the arch interrupt backend.


