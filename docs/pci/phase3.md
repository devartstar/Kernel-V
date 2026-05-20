# Phase 3 — PCI Resource Discovery and Enablement

## Purpose

Phase 3 takes Kernel-V from **PCI identity discovery** to **PCI resource interpretation**. After Phase 2, the kernel knows which PCI functions exist on bus 0 and can retain those identities in a registry. That is necessary, but it is not yet enough to use a device. A driver cannot do anything meaningful until the kernel can answer the following questions for a discovered PCI function:

* Which Base Address Registers (BARs) are implemented?
* Is each BAR an I/O-space aperture or a memory-space aperture?
* If memory-backed, is the BAR 32-bit or 64-bit?
* Is the memory BAR marked prefetchable?
* What normalized base address does the BAR describe?
* What does the PCI command register currently allow for this function?
* What must the kernel enable before software can legally touch the device’s resources?

This phase therefore builds the first **resource-discovery layer** of the PCI subsystem.

---

## Phase 3 Goal

By the end of Phase 3, Kernel-V should be able to:

* read and interpret BAR registers for discovered PCI functions
* normalize raw BAR encodings into structured kernel resource records
* distinguish I/O BARs from MMIO BARs correctly
* handle 64-bit BAR pairing correctly
* read and manipulate the PCI command register deliberately
* attach decoded resource state to each retained PCI function record
* dump the resource view in a stable format for validation

This phase is complete when the kernel can say, for each discovered function:

> "This function exists, these are the resources it decodes, and this is the command-register state governing access to those resources."

---

## Architectural Principles for Phase 3

### Principle 1 — Raw register access and semantic decoding must remain separate

The config-space access layer should continue doing only transport work. BAR decoding logic must not leak downward into `pci_cfg_read*()`. A BAR is not merely a 32-bit register value. It is a packed encoding of a resource type, attribute set, and base address. That interpretation belongs in the PCI resource layer, not the raw config layer.

### Principle 2 — Resource discovery must be attached to retained PCI records, not recomputed ad hoc

Once a PCI function is in the registry, Phase 3 should enrich that record with resource information. Later driver code should ask the registry for a function’s BAR state, not reread BARs every time from config space unless there is a very specific rescan policy.

### Principle 3 — Command-register enablement is policy, not incidental bit-twiddling

Enabling bus mastering, memory space, or I/O space is not a side effect that should happen casually. It changes how the device participates on the bus. The kernel must therefore expose deliberate helpers for command-register read/modify/write operations rather than open-coded bit updates scattered across future drivers.

### Principle 4 — Phase 3 is still discovery and interpretation, not full driver bring-up

This phase should not yet attempt queue setup, interrupt handler installation, capability walking, or driver binding. Those belong to later phases. The goal here is to create a truthful description of resources and enablement state.

---

# Subphase 3.1 — PCI BAR and Resource Data Model

## Purpose

Define the kernel-owned representation for PCI resources before reading or decoding any BARs.

## Why this comes first

If the kernel reads BAR registers before defining a stable output object, the first implementation will devolve into ad hoc bit masking and logging with no retained contract for later code.

## Main learning goals

By the end of this subphase, you should understand:

* why a BAR is not just a register value
* how BARs encode both address bits and attribute bits
* why the PCI registry record needs resource slots
* why raw BAR values and normalized base addresses should both be retained

## Technical goals

Create:

* `pci_bar_kind_t`
* `pci_bar_info_t`
* BAR arrays inside `pci_function_record_t`
* per-BAR initialization helpers

The resource object should capture at least:

* BAR index
* presence state
* resource kind (`unused`, `io`, `mem32`, `mem64`)
* raw low dword
* raw high dword for 64-bit BARs
* normalized base
* prefetchable flag

## Deliverable

A stable resource container attached to each PCI function record.

## Exit criteria

Every discovered PCI function in the registry has initialized BAR slots even before decode has been performed.

---

# Subphase 3.2 — Raw BAR Register Acquisition

## Purpose

Read BAR registers from config space for already-discovered PCI functions without yet performing full semantic decoding.

## Why this comes second

The kernel must first acquire the exact raw config contents before it can interpret them. Reading and decoding must remain distinct stages.

## Main learning goals

By the end of this subphase, you should understand:

* where BAR0–BAR5 live in a Type-0 header
* why a BAR read must preserve the raw register value
* why 64-bit BARs occupy two adjacent BAR slots
* why bridge headers must not be decoded with Type-0 BAR assumptions

## Technical goals

Implement helpers such as:

* `pci_read_bar_raw(record, index)`
* `pci_read_type0_bars(record)`

The implementation should:

* operate only on Type-0 header layout initially
* read BAR0 through BAR5 from config space
* store raw low/high values into the record
* leave semantic interpretation to the next subphase

## Deliverable

Kernel records now contain the exact raw BAR register contents for eligible functions.

## Exit criteria

For a known function, raw BAR values can be printed consistently across boots under the same machine configuration.

---

# Subphase 3.3 — BAR Semantic Decode

## Purpose

Interpret raw BAR register values into structured resource meaning.

## Why this is its own subphase

BARs combine attribute bits and address bits in one encoding. That decode logic is easy to get subtly wrong, especially when differentiating I/O BARs from MMIO BARs and handling 64-bit memory BAR pairs.

## Main learning goals

By the end of this subphase, you should understand:

* how bit 0 distinguishes I/O space from memory space
* how memory BAR type bits encode 32-bit vs 64-bit
* how the prefetchable bit applies only to memory BARs
* how low attribute bits must be masked off before normalizing the base
* why 64-bit BARs consume the next BAR slot

## Technical goals

Implement decode helpers such as:

* `pci_decode_bar(record, index)`
* `pci_decode_type0_bars(record)`

The implementation should:

* classify each BAR as `unused`, `io`, `mem32`, or `mem64`
* compute normalized base addresses
* capture prefetchability for memory BARs
* correctly consume BAR+1 for `mem64`
* avoid decoding a partner slot twice

## Deliverable

Each eligible PCI function record contains normalized BAR resource information.

## Exit criteria

For each BAR, the kernel can state its kind and normalized base, and 64-bit pair handling is correct.

---

# Subphase 3.4 — PCI Command/Status Register Model

## Purpose

Introduce a disciplined model for reading and interpreting the PCI command and status registers.

## Why this comes before enablement

The kernel must understand current command/status state before it starts modifying device enable bits.

## Main learning goals

By the end of this subphase, you should understand:

* what the PCI command register controls at a high level
* why memory space enable, I/O space enable, and bus mastering matter
* why status bits are observational rather than enable policy
* why command-register updates must be read-modify-write operations

## Technical goals

Create:

* command/status register constants
* helpers to read command and status fields
* a small command-state structure or accessor set

Implement helpers such as:

* `pci_read_command(bdf)`
* `pci_write_command(bdf, value)`
* `pci_update_command_bits(bdf, set_mask, clear_mask)`

## Deliverable

A clean command/status access layer that future code can use safely.

## Exit criteria

The kernel can read and report current command/status state for a discovered function without open-coded register arithmetic.

---

# Subphase 3.5 — Device Enablement Policy

## Purpose

Define and implement the first policy-level helpers that enable a device’s resource decoding in a deliberate, reviewable way.

## Why this is a separate subphase

Reading BARs and understanding command bits are descriptive operations. Enabling memory space, I/O space, or bus mastering is a control decision and therefore deserves its own explicit interface.

## Main learning goals

By the end of this subphase, you should understand:

* why touching MMIO or I/O resources before the corresponding command bit is enabled is architecturally wrong
* why bus mastering must be enabled deliberately rather than incidentally
* why enablement helpers should be narrow and intention-revealing

## Technical goals

Implement helpers such as:

* `pci_enable_mem_space(bdf)`
* `pci_enable_io_space(bdf)`
* `pci_enable_bus_master(bdf)`
* `pci_disable_*` counterparts if you want symmetric policy

These should use command-register read-modify-write helpers rather than direct open-coded writes.

## Deliverable

A deliberate command-register policy layer for later drivers.

## Exit criteria

The kernel can report the command register before and after enablement and demonstrate only the intended bits changed.

---

# Subphase 3.6 — Attach Resource State to the PCI Registry

## Purpose

Integrate BAR discovery and command-state awareness into retained PCI records so later phases consume one coherent PCI function object.

## Why this comes here

Once decode and enablement helpers exist, the registry records should be enriched so that future code does not need to re-query hardware repeatedly.

## Main learning goals

By the end of this subphase, you should understand:

* why resource interpretation belongs with the retained function record
* how registry enrichment differs from initial enumeration
* why phase markers such as `bars_valid` or `resources_valid` are useful

## Technical goals

Extend `pci_function_record_t` with:

* decoded BAR info
* optional cached command/status fields
* validity markers for Phase 3-enriched state

Implement a pass such as:

* `pci_enrich_record_resources(record)`
* `pci_enrich_registry_resources(reg)`

## Deliverable

The PCI registry evolves from an identity registry into a resource-aware registry.

## Exit criteria

Later code can inspect a registry record and determine both identity and resource information without rescanning hardware.

---

# Subphase 3.7 — Resource Dump and External Validation

## Purpose

Create a stable resource dump so that the newly discovered BAR and command-state information can be validated repeatedly.

## Why this matters

BAR work is bit-level and easy to get subtly wrong. A disciplined dump makes early validation dramatically easier.

## Main learning goals

By the end of this subphase, you should understand:

* how to print BAR state in a readable but precise form
* how to show `io` vs `mem32` vs `mem64`
* how to compare your dump against `lspci -vv` or QEMU monitor output
* how to identify likely decode bugs from output anomalies

## Technical goals

Add routines such as:

* `pci_dump_bar_info(record)`
* `pci_dump_registry_resources(reg)`

Expected output should include:

* BDF
* vendor/device identity
* BAR index
* BAR kind
* normalized base
* prefetchability for memory BARs
* current command register state

## Deliverable

A repeatable resource dump suitable for manual validation.

## Exit criteria

You can compare the kernel’s decoded BAR view against external tooling and explain any differences.

---

# Subphase 3.8 — Phase 3 Validation and Known Limits

## Purpose

Freeze what the PCI core now knows about function resources and what it still does not know.

## Why this matters

Without a clear end-of-phase boundary, later driver code will start assuming support for things like BAR sizing, capability walking, or bridge window decoding before those features exist.

## Main learning goals

By the end of this subphase, you should understand:

* what resource information is now retained
* what command policy is now available
* what still remains for Phase 4 and beyond

## Technical goals

Document that Phase 3 supports:

* raw BAR register acquisition for Type-0 headers
* BAR semantic decode for I/O, 32-bit memory, and 64-bit memory BARs
* command-register read/modify/write helpers
* first device enablement helpers
* retained resource state in the PCI registry

Document that Phase 3 does not yet support:

* BAR sizing through write-all-ones probing
* bridge window decoding
* ROM BAR interpretation
* capability-list walking
* MSI/MSI-X
* interrupt routing interpretation
* driver binding and per-device initialization

## Deliverable

A clean contract between Phase 3 and Phase 4.

## Exit criteria

You can state exactly what a PCI function record now contains and what later subsystems must still discover.

---

## Recommended implementation order

Follow this order strictly:

1. **Subphase 3.1 — PCI BAR and Resource Data Model**
2. **Subphase 3.2 — Raw BAR Register Acquisition**
3. **Subphase 3.3 — BAR Semantic Decode**
4. **Subphase 3.4 — PCI Command/Status Register Model**
5. **Subphase 3.5 — Device Enablement Policy**
6. **Subphase 3.6 — Attach Resource State to the PCI Registry**
7. **Subphase 3.7 — Resource Dump and External Validation**
8. **Subphase 3.8 — Phase 3 Validation and Known Limits**

This order matters because:

* the data model must exist before raw BAR reads have anywhere stable to land
* raw acquisition must happen before semantic decode
* semantic decode must happen before enablement policy can be judged meaningfully
* registry enrichment must wait until both decode and command-state helpers exist
* validation output only becomes trustworthy after all retained state is available

---

## What Phase 3 is really building

Phase 3 builds three new contracts on top of the Phase 2 identity-discovery core.

### Contract A — Resource contract

The kernel can say what resource apertures a PCI function exposes.

### Contract B — Enablement contract

The kernel can say what the PCI command register currently permits and can change that state deliberately.

### Contract C — Retained resource state contract

The kernel can retain and serve both identity and resource information from the PCI registry.

Those three contracts are what later make MMIO mapping, I/O port access, interrupt setup, and real driver bring-up possible.

---

## Definition of done for Phase 3

Phase 3 is complete only when:

* Type-0 BARs are read and decoded correctly
* 64-bit BAR pairing is handled correctly
* I/O-space and memory-space BARs are distinguished correctly
* command-register helpers exist and behave as deliberate policy interfaces
* decoded BAR/resource state is retained in the PCI registry
* the kernel can dump and validate this resource view repeatably

At that point, Kernel-V no longer merely knows **which PCI functions exist**. It knows **what resources those functions decode and what bus-level enablement state governs access to those resources**.

---