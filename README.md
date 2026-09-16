# Kernel-V (v0.8.x) PCI Learning Subproject Roadmap
## Project Helix — PCI Core + VirtIO Block for Kernel-V

---

# 0. Project Intent

## Main Goal
Build a **real PCI subsystem** inside Kernel-V and prove it by bringing up a **VirtIO Block PCI device**, so the kernel gains:

- deep understanding of PCI device discovery and configuration
- a reusable bus/device/driver foundation
- real storage access through a modern PCI device
- a launchpad for future filesystems, AHCI, NICs, and more advanced drivers

## Why this project matters
This is not just a “learn PCI” project.

It upgrades the kernel in a meaningful way by introducing:

- hardware resource discovery
- device enumeration
- MMIO / PIO resource handling
- driver binding
- DMA-oriented memory thinking
- interrupt-driven device completion
- the first real block device path

---

# 1. Big Picture Architecture

The project is divided into these phases:

1. **Foundation Hardening for Device Work**
2. **PCI Config Access + Enumeration**
3. **PCI Resources, BARs, and Capabilities**
4. **Kernel Device / Driver Model**
5. **VirtIO PCI Transport Bring-up**
6. **Virtqueue and DMA-style Queue Setup**
7. **VirtIO Block Driver**
8. **Kernel Block Layer Integration**
9. **Validation, Debugging, and Extension Path**

Each phase has:
- purpose
- technical learning goals
- implementation goals
- expected deliverables
- mistakes to avoid
- exit criteria

---

# Phase 1 — Foundation Hardening for Device Work

## Purpose
Before touching PCI devices, make the kernel reliable for hardware-facing code.

PCI/device work exposes weaknesses in:
- physical memory assumptions
- page allocation correctness
- MMIO access discipline
- interrupt safety
- driver structure cleanliness

## What you should learn
By the end of this phase, you should understand:

- why hardware drivers care about **physical addresses**, not just kernel virtual addresses
- why **page alignment** and **contiguity** matter
- why device code should not call raw PMM functions everywhere
- why MMIO needs dedicated typed access helpers
- why lockless driver code becomes dangerous once interrupts are involved

## Technical goals
Implement or improve:

### 1. Physical allocator hardening
Add support for:

- page-aligned page allocation
- allocation of multiple contiguous pages
- reliable free path for multi-page allocations
- ability to retrieve the physical address of an allocated kernel buffer

### 2. DMA-style allocation abstraction
Add a minimal interface like:

- `void* dma_alloc_pages(size_t count, phys_addr_t* out_phys)`
- `void dma_free_pages(void* kva, size_t count)`

Even if the implementation is simple at first, the abstraction is important.

### 3. MMIO access helpers
Introduce helpers for mapped device memory:

- `mmio_read8`
- `mmio_read16`
- `mmio_read32`
- `mmio_write8`
- `mmio_write16`
- `mmio_write32`

Also keep a clean distinction between:

- port I/O
- MMIO access

### 4. Tiny locking layer
Introduce a minimal spinlock API:

- `spin_lock`
- `spin_unlock`
- `spin_lock_irqsave`
- `spin_unlock_irqrestore`

### 5. Interrupt registration abstraction
Add a device-facing interrupt hook layer, even if it still uses legacy PIC/INTx under the hood.

## Deliverables
At the end of this phase, you should have:

- reliable page allocation for device buffers
- a DMA-style allocation API
- MMIO helper layer
- tiny spinlock support
- a minimal interrupt registration abstraction

## Mistakes to avoid
- hardcoding assumptions that “kernel virtual == physical”
- letting drivers directly manipulate random paging internals
- mixing port I/O and MMIO access casually
- writing device code without IRQ-safe locking discipline

## Exit criteria
You are ready to move on when:

- you can allocate contiguous, page-aligned memory and know both its virtual and physical addresses
- you can safely read/write MMIO through helpers
- you have enough locking and IRQ abstraction for future driver code

---

# Phase 2 — PCI Config Space Access + Enumeration

## Purpose
Build the kernel’s first real PCI core: discover every PCI function visible to the machine.

## What you should learn
By the end of this phase, you should understand:

- how x86 PCI Configuration Mechanism #1 works
- why PCI devices are addressed as:
  - bus
  - device
  - function
- how a device exposes:
  - vendor ID
  - device ID
  - class code
  - subclass
  - programming interface
  - revision ID
  - header type
- what multifunction devices are
- how PCI enumeration actually begins

## Technical goals
Implement:

### 1. PCI config read/write primitives
Using `0xCF8` / `0xCFC`:

- `pci_config_read8`
- `pci_config_read16`
- `pci_config_read32`
- optionally write variants too

### 2. Address construction logic
Encode:
- bus
- device
- function
- register offset

into config address cycles correctly.

### 3. PCI function scan logic
Start with:
- bus 0
- devices 0–31
- functions 0–7 as needed

Later make scanning more complete.

### 4. Device identification structure
Create a structure like:

- bus
- device
- function
- vendor ID
- device ID
- class code
- subclass
- prog-if
- revision
- header type

### 5. Kernel PCI registry
Store discovered devices in a central table/list.

## Deliverables
At the end of this phase, the kernel should:

- scan PCI bus/device/function space
- identify present devices
- log all discovered devices
- maintain an internal list of PCI functions

## Suggested milestone output
Your kernel log should print something conceptually like:

- `00:00.0 host bridge ...`
- `00:01.0 ISA bridge ...`
- `00:02.0 VGA compatible controller ...`
- `00:04.0 VirtIO block ...`

## Mistakes to avoid
- assuming every device is single-function
- forgetting to validate vendor ID `0xFFFF`
- mixing device-level and function-level thinking
- building logs only, without an internal data model

## Exit criteria
You are ready to move on when:

- config reads are correct
- device discovery works
- discovered PCI devices are stored internally, not just printed

---

# Phase 3 — PCI Resources, BARs, and Capabilities

## Purpose
Move from “device exists” to “device owns resources and may be usable.”

## What you should learn
By the end of this phase, you should understand:

- what a BAR is
- the difference between:
  - I/O BAR
  - MMIO BAR
- how BAR sizing works
- why BAR values are resource descriptors, not raw addresses to trust blindly
- how PCI command register bits enable:
  - I/O space
  - memory space
  - bus mastering
- how the PCI capability linked list works

## Technical goals
Implement:

### 1. BAR parsing
For each standard device header, decode BARs:

- determine whether BAR is I/O or MMIO
- determine base address
- determine resource length by probing size
- store all BAR information in resource descriptors

### 2. PCI command/status handling
Add helpers to:

- read command register
- set/clear bits
- enable:
  - memory space
  - I/O space
  - bus mastering

### 3. Capability walking
If the device advertises capabilities:

- locate capability pointer
- walk the linked list
- record capability IDs and offsets

This becomes very important for VirtIO PCI and later MSI/MSI-X.

## Deliverables
At the end of this phase, the kernel should know for each PCI function:

- what resources it owns
- how large each BAR is
- whether the device exposes capabilities
- whether the device can be enabled for bus mastering

## Mistakes to avoid
- treating BAR contents as final before proper masking/decoding
- ignoring capability chains
- enabling bus mastering too casually without understanding why
- assuming all devices use only MMIO

## Exit criteria
You are ready to move on when:

- BARs are decoded into structured resource objects
- capability walking works
- you can enable a PCI device cleanly

---

# Phase 4 — Kernel Device / Driver Model

## Purpose
Prevent PCI from turning into a pile of special cases.

This phase creates the first reusable driver framework.

## What you should learn
By the end of this phase, you should understand:

- why kernels separate:
  - discovered device
  - driver implementation
  - driver binding/probe
- how a bus core hands a discovered function to a matching driver
- why future scalability depends on not hardcoding probe logic all over the place

## Technical goals
Introduce minimal kernel objects:

### 1. Generic device structure
Contains things like:
- name
- parent bus
- device type
- private driver data pointer

### 2. PCI device structure
Wraps:
- bus/device/function addressing
- IDs
- class fields
- BAR resources
- capabilities
- IRQ metadata

### 3. PCI driver structure
Contains:
- name
- match information
- `probe()`
- optional `remove()`

### 4. Match and bind flow
Support at least:
- vendor/device match
- optionally class/subclass match

### 5. Helper APIs
Provide helpers like:
- enable device
- claim/map BAR
- register interrupt callback

## Deliverables
At the end of this phase:

- a PCI driver can be registered
- the PCI core can probe matching devices
- the driver gets handed a clean `pci_device` object

## Mistakes to avoid
- letting drivers rescan PCI on their own
- storing PCI config-space facts in ad hoc globals
- writing VirtIO code directly inside the PCI core
- skipping abstraction because “only one driver exists for now”

## Exit criteria
You are ready to move on when:

- a PCI driver registration/bind path exists
- the PCI subsystem can attach a driver to a discovered device

---

# Phase 5 — VirtIO PCI Transport Bring-up

## Purpose
Start the first real PCI device target: VirtIO Block over PCI.

This phase is about transport discovery and device negotiation, not block I/O yet.

## What you should learn
By the end of this phase, you should understand:

- what VirtIO is conceptually
- why VirtIO over PCI is a transport over PCI resources
- the difference between:
  - PCI enumeration
  - VirtIO transport discovery
  - VirtIO device-specific logic
- how modern VirtIO PCI exposes capabilities for:
  - common config
  - notify config
  - ISR status
  - device config

## Technical goals
Implement:

### 1. Match the VirtIO block PCI function
Usually via vendor/device or class-based recognition depending on setup.

### 2. Parse modern VirtIO PCI capabilities
Find and store mappings for:
- common configuration
- notification region
- ISR status
- device-specific configuration

### 3. MMIO map the required regions
Map the BAR backing those structures.

### 4. Device status progression
Implement the initial state machine:
- reset
- acknowledge
- driver
- feature negotiation
- features OK
- driver OK

### 5. Feature negotiation
Read device features and accept the subset your kernel supports.

## Deliverables
At the end of this phase, the kernel should:

- discover a VirtIO block PCI device
- attach the VirtIO block driver
- parse and map the required VirtIO PCI structures
- complete initial device negotiation successfully

## Mistakes to avoid
- mixing transport setup with queue setup too early
- negotiating features you do not actually support
- hardcoding config layout without capability parsing
- failing to separate generic VirtIO transport code from block-specific code

## Exit criteria
You are ready to move on when:

- the VirtIO block device reaches “driver OK” cleanly
- all required VirtIO transport regions are discovered and mapped

---

# Phase 6 — Virtqueue and DMA-Style Queue Setup

## Purpose
Create the shared queue structures that allow the kernel and device to exchange work.

## What you should learn
By the end of this phase, you should understand:

- what a virtqueue is
- how descriptor tables, available rings, and used rings work
- why these structures must be physically accessible to the device
- why cache/ordering thinking matters even in “simple” drivers
- how notification and completion flow works

## Technical goals
Implement:

### 1. Virtqueue memory allocation
Allocate physically accessible, properly aligned memory for:
- descriptor table
- available ring
- used ring

### 2. Queue initialization
Set queue size
Bind queue memory
Enable queue

### 3. Descriptor management
Build descriptors for request chains.

### 4. Notification path
Notify the device when new work is available.

### 5. Completion path
Track used ring updates and recognize completion.

## Deliverables
At the end of this phase, the kernel should:

- create at least one functioning virtqueue
- post a descriptor chain
- see the device consume it
- observe used-ring completion

## Mistakes to avoid
- allocating queue memory without knowing physical address
- confusing descriptor lifetime and request lifetime
- ignoring ordering / visibility assumptions
- trying to support many queues immediately

## Exit criteria
You are ready to move on when:

- one queue is initialized correctly
- requests can be submitted and later seen as completed

---

# Phase 7 — VirtIO Block Driver

## Purpose
Turn the transport and queue machinery into actual block I/O.

## What you should learn
By the end of this phase, you should understand:

- how a block request is represented
- how LBA-based I/O maps into device requests
- how request headers, data buffers, and status bytes are chained
- how completion confirms success/failure
- how an actual storage driver interacts with a queue-based PCI device

## Technical goals
Implement:

### 1. Read-only path first
Start with:
- single-sector read
- fixed scratch buffer
- synchronous completion

Then extend to:
- multi-sector reads

### 2. Write path
After reads are stable:
- implement sector writes
- validate persistence

### 3. Capacity query
Read the device’s exposed geometry/capacity info.

### 4. Error handling
Handle:
- unsupported requests
- queue submission failure
- device error status

## Deliverables
At the end of this phase, the kernel should:

- read sectors from the VirtIO block device
- optionally write sectors back
- verify that returned data matches expected disk contents

## Suggested validation targets
- read MBR / GPT sector
- dump known sector contents
- compare with a prepared disk image
- read multiple sequential sectors

## Mistakes to avoid
- starting with async complexity too early
- trying to build a cache before raw I/O works
- burying request logic inside interrupt handlers
- not checking returned request status carefully

## Exit criteria
You are ready to move on when:

- sector reads are correct and repeatable
- write path is either working or intentionally deferred with clean design

---

# Phase 8 — Kernel Block Layer Integration

## Purpose
Make the driver useful to the rest of the kernel.

## What you should learn
By the end of this phase, you should understand:

- why drivers should expose generic services upward
- why upper layers should not depend on VirtIO internals
- how a block device abstraction becomes a kernel-wide interface

## Technical goals
Introduce a tiny block layer:

### 1. Block device abstraction
Something like:
- sector size
- capacity
- read op
- write op
- private driver pointer

### 2. Registration
Allow block devices to be registered centrally.

### 3. Generic block API
Support:
- `block_read(dev, lba, count, buf)`
- `block_write(dev, lba, count, buf)`

### 4. Optional request serialization
Keep the first design simple and synchronous if needed.

## Deliverables
At the end of this phase:

- other kernel components can use storage without knowing about VirtIO PCI
- the first true storage service boundary exists inside Kernel-V

## Mistakes to avoid
- leaking VirtIO-specific structures into higher-level code
- overengineering a full Linux-style block layer too early
- mixing test code with permanent API shape

## Exit criteria
You are ready to move on when:

- a generic block API exists
- VirtIO block is one implementation behind it

---

# Phase 9 — Validation, Debugging, and Extension Path

## Purpose
Stabilize what you built and prepare for future device work.

## What you should learn
By the end of this phase, you should understand:

- how to debug device-driver failures systematically
- how to verify PCI resource correctness
- how to distinguish:
  - enumeration bugs
  - MMIO mapping bugs
  - queue bugs
  - interrupt bugs
  - request formatting bugs

## Technical goals
Add debugging and validation tools:

### 1. PCI dump tooling
Print:
- full config header
- BARs
- command/status
- capabilities

### 2. VirtIO dump tooling
Print:
- negotiated features
- queue size
- queue addresses
- device status
- ISR status

### 3. Block verification helpers
- sector hex dump
- compare against expected signatures
- repeated-read consistency tests

### 4. Interrupt/queue tracing
Log:
- submit index
- notify
- ISR hit
- used ring advance
- completion status

## Deliverables
You finish this phase with a subsystem that is not just working once, but explainable and debuggable.

## Exit criteria
You are done when:

- PCI enumeration is stable
- VirtIO block read path is stable
- block abstraction exists
- the subsystem is understandable enough to extend later

---

# 2. Learning Progression Summary

## What each phase teaches conceptually

### Phase 1
**Device foundations**
- physical memory discipline
- DMA-oriented thinking
- MMIO safety
- lock/IRQ safety

### Phase 2
**PCI discovery**
- config space
- BDF addressing
- enumeration

### Phase 3
**PCI usability**
- BARs
- resources
- command register
- capabilities

### Phase 4
**Kernel architecture**
- bus/device/driver separation
- probe model

### Phase 5
**Transport-level device bring-up**
- VirtIO PCI structures
- feature negotiation
- device initialization state machine

### Phase 6
**Shared queues**
- descriptors
- avail/used rings
- physically accessible memory

### Phase 7
**Real storage I/O**
- block request construction
- data transfer
- completion handling

### Phase 8
**Subsystem design**
- block layer abstraction
- driver independence

### Phase 9
**Reliability and extension**
- debugging discipline
- testability
- readiness for future drivers

---

# 3. Recommended Milestones

## Milestone A
PCI enumeration works.

## Milestone B
BAR decoding and capability walking work.

## Milestone C
PCI driver model attaches a VirtIO block device.

## Milestone D
VirtIO negotiation succeeds.

## Milestone E
One virtqueue is live.

## Milestone F
Single-sector read succeeds.

## Milestone G
Block layer exists and uses VirtIO block as backend.

---

# 4. Recommended “Definition of Done”

This subproject is considered successful when the kernel can:

- enumerate PCI devices
- decode and manage PCI BAR resources
- bind a PCI driver through a structured PCI core
- bring up a VirtIO PCI block device
- submit and complete real block I/O requests
- expose the device behind a generic block interface

---

# 5. After This Project

Once this project is done, your next strong options are:

## Option 1 — Filesystem
Use the block layer to build:
- sector cache
- simple filesystem
- inode/file abstraction

## Option 2 — AHCI
Use the PCI core to attach a real SATA/AHCI controller.

## Option 3 — NIC
Use the PCI core to attach a network card and begin networking.

## Option 4 — MSI/MSI-X and APIC path
Upgrade interrupt delivery beyond legacy PIC/INTx.

---
