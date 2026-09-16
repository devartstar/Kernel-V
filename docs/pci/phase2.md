# Phase 2 — PCI Config Access + Enumeration

## Structured subphase breakdown

```markdown
# Phase 2 — PCI Config Access + Enumeration

## Phase 2 Goal

Build the kernel's first real PCI discovery core by teaching Kernel-V how to:

- access PCI configuration space through x86 mechanism #1
- address PCI functions using bus/device/function coordinates
- identify which PCI functions actually exist
- read the key identity fields from each function
- store discovered functions in a kernel-owned PCI registry

This phase is complete when the kernel can scan PCI space, detect valid functions, log them accurately, and retain the discovered information in structured kernel data rather than only printing it.

---

# Subphase 2.1 — PCI Mental Model and Addressing Contract

## Purpose

Establish the exact conceptual model of PCI before writing access code.

This subphase answers these questions precisely:

- what is a PCI bus?
- what is a device number?
- what is a function number?
- why does PCI use B:D:F instead of just one flat identifier?
- what is configuration space?
- why can a single slot expose multiple functions?
- what is the difference between a PCI device and a PCI function?

## Why this comes first

If you do not separate "device" from "function" in your head, your enumeration logic will be wrong almost immediately.

Most beginner PCI code accidentally treats:
- one device number
as
- one logical function

That is incorrect.

A PCI device location is:
- bus number
- device number
- function number

The function is the actual configuration-space endpoint you enumerate.

## Main learning goals

By the end of this subphase, you should understand:

- PCI hierarchy begins with buses
- each bus has up to 32 device slots
- each device slot can expose up to 8 functions
- configuration space is per-function
- vendor ID / device ID belong to a function's config space
- multifunction devices are detected through the header type

## Technical deliverable

A written internal contract for Kernel-V defining:

- `pci_bus_t`
- `pci_device_slot`
- `pci_function`
- BDF notation and formatting
- the fact that the kernel enumerates functions, not abstract slots

## Exit criteria

You can state, without ambiguity:
- what `00:04.0` means
- why `00:04.3` can exist even if `00:04.1` and `00:04.2` do not
- why config reads are performed per function
```

---

```markdown
# Subphase 2.2 — x86 PCI Configuration Mechanism #1 Access Layer

## Purpose

Implement the raw hardware access path to PCI configuration space using:

- CONFIG_ADDRESS port `0xCF8`
- CONFIG_DATA port `0xCFC`

This is the lowest software layer of PCI enumeration.

## Why this comes second

Before scanning buses, the kernel must first know how to issue one correct config-space transaction.

If this layer is wrong, every later PCI result will be garbage.

## Main learning goals

By the end of this subphase, you should understand:

- what PCI Configuration Mechanism #1 is
- why the CPU uses I/O ports instead of MMIO here
- how the 32-bit CONFIG_ADDRESS value is constructed
- why config register offsets must be aligned to 32-bit boundaries for mechanism #1
- how 8-bit, 16-bit, and 32-bit reads are derived from 32-bit aligned accesses

## Technical goals

Implement a raw access layer with functions like:

- `pci_cfg_addr_make(bus, dev, fn, reg)`
- `pci_cfg_read32(bus, dev, fn, reg)`
- `pci_cfg_read16(bus, dev, fn, reg)`
- `pci_cfg_read8(bus, dev, fn, reg)`
- optional write variants too

## Important design rule

This layer should be completely dumb.

It should not know about:
- vendor IDs
- enumeration policy
- multifunction rules
- device registry

It should only know:
- how to construct config transactions
- how to extract the requested width

## Deliverable

A tiny hardware access module whose only job is correct config-space reads and writes.

## Exit criteria

You can manually read:
- vendor/device ID at offset `0x00`
- class/subclass/prog-if/revision at offset `0x08`
- header type at offset `0x0C`

for a known BDF and get stable values.
```

---

```markdown
# Subphase 2.3 — PCI Configuration Header Field Definitions

## Purpose

Turn raw offsets into readable and correct PCI semantics.

This subphase defines the constants, offsets, and masks that make config space understandable.

## Why this comes before scanning

If your scanner uses magic offsets everywhere, the code will become unreadable and Phase 3 will become painful.

The kernel needs a formal vocabulary for:
- standard config fields
- header type extraction
- class code extraction
- vendor ID invalid sentinel checks

## Main learning goals

By the end of this subphase, you should understand:

- layout of the standard Type 0 config header at a high level
- which fields are common to all PCI functions
- how vendor/device ID share offset `0x00`
- how revision/prog-if/subclass/class share offset `0x08`
- how header type lives in the upper byte of offset `0x0C`
- why vendor ID `0xFFFF` means "no function present"

## Technical goals

Create constants/macros for at least:

- `PCI_CFG_VENDOR_ID`
- `PCI_CFG_DEVICE_ID`
- `PCI_CFG_COMMAND`
- `PCI_CFG_STATUS`
- `PCI_CFG_REVISION_ID`
- `PCI_CFG_PROG_IF`
- `PCI_CFG_SUBCLASS`
- `PCI_CFG_CLASS_CODE`
- `PCI_CFG_HEADER_TYPE`

Also define extraction helpers or macros.

## Deliverable

A clean `pci_regs.h` or equivalent header that replaces magic numbers in future PCI code.

## Exit criteria

Your raw access layer plus register definitions are enough to write readable config queries without hardcoded byte offsets scattered across the scanner.
```

---

```markdown
# Subphase 2.4 — Single-Function Probe Path

## Purpose

Build the smallest correct enumeration slice by probing one known BDF path.

This is the first bridge from raw access to discovery logic.

## Why this comes before full scanning

You do not want to debug:
- access layer
- field extraction
- full-bus scan logic
- multifunction traversal

all at once.

The right method is:
1. probe one function
2. validate the fields
3. then generalize

## Main learning goals

By the end of this subphase, you should understand:

- how to determine whether a PCI function exists
- how to read and interpret vendor/device ID
- how to fetch class code, subclass, prog-if, revision, and header type for one function
- how to format one discovered function in a useful diagnostic line

## Technical goals

Implement a helper like:

- `pci_probe_function(bus, dev, fn, struct pci_function_info *out)`

It should:
- read vendor ID
- reject `0xFFFF`
- if present, populate a small identity structure

## Deliverable

A probe function that can successfully identify one real PCI function and print something like:

- `00:04.0 vendor=0x1af4 device=0x1001 class=0x01 subclass=0x00 progif=0x00 hdr=0x00`

## Exit criteria

The kernel can probe one chosen BDF deterministically and return a structured identity object instead of ad hoc logs only.
```

---

```markdown
# Subphase 2.5 — Multifunction Detection Logic

## Purpose

Teach the kernel the rule that determines whether function numbers `1..7` must be scanned for a device slot.

## Why this is its own subphase

This logic is easy to write incorrectly and it changes the scan strategy.

If you always scan all 8 functions everywhere, the code still works but becomes noisy and inefficient.
If you never scan function `1..7`, you will miss real hardware.

So the kernel must learn the exact multifunction rule.

## Main learning goals

By the end of this subphase, you should understand:

- function 0 is special for probe policy
- header type bit 7 indicates multifunction capability
- if function 0 is absent, the slot is typically treated as absent for enumeration purposes
- if function 0 exists and multifunction bit is clear, only function 0 should be scanned
- if multifunction bit is set, functions `1..7` must be probed individually

## Technical goals

Implement a helper like:

- `pci_slot_is_multifunction(bus, dev)`

or embed the policy into the scanner cleanly.

## Deliverable

The kernel has correct per-slot function scan policy and will not miss multifunction devices.

## Exit criteria

You can explain and correctly implement why:
- some slots are scanned only at function 0
- some slots require probing function 1 through 7
```

---

```markdown
# Subphase 2.6 — Full Bus 0 Enumeration Engine

## Purpose

Generalize from one probe to a complete scan of bus 0.

This is the first real enumerator.

## Why bus 0 first

Bus 0 is the correct first milestone because it gives you:
- immediate visible hardware
- simpler logic
- lower debugging complexity

Do not start with recursive bus discovery yet.

## Main learning goals

By the end of this subphase, you should understand:

- how a scanner iterates device slots 0..31
- how multifunction policy changes the inner function scan
- why enumeration is fundamentally "try and see if vendor ID is valid"
- how to avoid confusing "slot exists" with "function exists"

## Technical goals

Implement a scanner like:

- `pci_scan_bus_0()`

It should:
- iterate device numbers 0..31
- probe function 0
- if present and multifunction, probe functions 1..7
- emit structured results for each present function

## Deliverable

A stable enumeration of all PCI functions visible on bus 0.

## Exit criteria

Kernel logs show all present functions on bus 0, and the result set is stable across boots under the same QEMU setup.
```

---

```markdown
# Subphase 2.7 — Internal PCI Function Registry

## Purpose

Stop treating enumeration as just printing and start treating it as kernel-owned discovered state.

This is the moment the PCI layer becomes a subsystem instead of a log generator.

## Why this comes here

Once bus 0 scanning works, the next mistake would be to continue relying on logs only.

Phase 3 and Phase 4 need a real registry of discovered PCI functions.

## Main learning goals

By the end of this subphase, you should understand:

- why logs are not enough
- why the kernel needs a retained representation of discovered hardware
- what minimum identity fields must be stored immediately
- what can wait until Phase 3

## Technical goals

Create a PCI function structure containing at least:

- bus
- device
- function
- vendor ID
- device ID
- class code
- subclass
- prog-if
- revision ID
- header type

Then create a registry:
- fixed array for now, or
- simple linked list if you prefer

A fixed array is perfectly fine for the first version.

## Deliverable

The PCI core stores discovered functions in kernel memory for later use.

## Exit criteria

After enumeration, another kernel subsystem can iterate the PCI function registry without rescanning hardware.
```

---

```markdown
# Subphase 2.8 — PCI Dump and Validation Output

## Purpose

Create a clean diagnostic output path so that PCI enumeration can be validated repeatedly and trusted.

## Why this matters now

Enumeration code often "works" but remains hard to inspect. Debugging early PCI without disciplined dumps is painful.

This subphase gives you the first real inspection tool.

## Main learning goals

By the end of this subphase, you should understand:

- why stable formatted logs matter for hardware bring-up
- how to make output readable in BDF notation
- how class/subclass information becomes more understandable with light decoding
- how to compare your output against QEMU expectations or `lspci`

## Technical goals

Add a dump routine like:

- `pci_dump_registry()`

It should print lines such as:

- `00:00.0 vendor=8086 device=1237 class=06 subclass=00 progif=00 hdr=00`
- `00:01.0 vendor=8086 device=7000 class=06 subclass=01 progif=00 hdr=80`
- `00:04.0 vendor=1af4 device=1001 class=01 subclass=00 progif=00 hdr=00`

Optionally add lightweight class-name decoding for common classes.

## Deliverable

A repeatable PCI discovery log that is structured enough to debug and compare externally.

## Exit criteria

You can boot the kernel, inspect the PCI dump, and confidently say:
- which functions were found
- which are multifunction
- which class codes they belong to
```

---

```markdown
# Subphase 2.9 — Phase 2 Validation and Known-Limit Documentation

## Purpose

Finish Phase 2 by locking in what the PCI layer can do now and what it deliberately does not do yet.

## Why this subphase matters

If you do not explicitly document the limits of the enumerator, later phases will silently rely on capabilities it does not yet have.

## Main learning goals

By the end of this subphase, you should understand:

- what your Phase 2 PCI core supports
- what remains for Phase 3
- what enumeration assumptions are still intentionally simple

## Technical goals

Document and validate that Phase 2 currently supports:

- config-space access through mechanism #1
- function discovery on bus 0
- multifunction-aware slot scanning
- structured per-function identity storage

Document that it does not yet support:

- recursive bridge-based bus discovery
- BAR decoding
- command/status manipulation
- capability walking
- IRQ routing interpretation
- driver binding

## Deliverable

A clear boundary between Phase 2 and Phase 3.

## Exit criteria

You can state precisely:
- what data the registry contains
- what is not yet known about devices
- why Phase 3 is the next required step
```

---

# Recommended implementation order inside Phase 2

This is the order we should follow strictly:

1. **Subphase 2.1 — PCI Mental Model and Addressing Contract**
2. **Subphase 2.2 — x86 PCI Configuration Mechanism #1 Access Layer**
3. **Subphase 2.3 — PCI Configuration Header Field Definitions**
4. **Subphase 2.4 — Single-Function Probe Path**
5. **Subphase 2.5 — Multifunction Detection Logic**
6. **Subphase 2.6 — Full Bus 0 Enumeration Engine**
7. **Subphase 2.7 — Internal PCI Function Registry**
8. **Subphase 2.8 — PCI Dump and Validation Output**
9. **Subphase 2.9 — Phase 2 Validation and Known-Limit Documentation**

That order is important because:

* 2.1 prevents conceptual bugs
* 2.2 and 2.3 make raw access correct and readable
* 2.4 validates one probe before scaling
* 2.5 fixes slot/function policy
* 2.6 performs the first real scan
* 2.7 turns scanning into retained subsystem state
* 2.8 makes it debuggable
* 2.9 prevents Phase 3 confusion

