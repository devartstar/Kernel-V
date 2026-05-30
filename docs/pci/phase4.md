## Phase 4 goal

Build the first reusable **kernel device/driver framework** on top of the PCI core, so the kernel can:

* convert retained PCI records into **PCI device objects**
* let drivers register **match rules + probe callbacks**
* bind a matching driver to a discovered PCI function
* give the driver a clean object instead of raw config-space logic
* prepare for **VirtIO PCI transport bring-up** in Phase 5 

## Phase 4 — structured subphases


1. **4.1 — Generic device model**
2. **4.2 — PCI device object**
3. **4.3 — PCI driver object**
4. **4.4 — Driver registry and bind/probe flow**
5. **4.5 — PCI driver helper API**
6. **4.6 — Validation with one dummy PCI driver**

### 4.1 — Generic device model

This subphase creates the smallest common kernel object that every future bus-backed device can embed or build on.

It should define a `device_t` or equivalent base object with fields such as:

* stable kernel-visible name
* device type / bus type
* parent pointer
* bound driver pointer
* private driver data pointer
* state flags such as discovered / bound / failed

The point is not abstraction for abstraction’s sake. The point is to stop the kernel from treating every discovered PCI function as a loose record in a table. A driver should bind to a **device object**, not directly to a registry slot.

### 4.2 — PCI device object

This subphase wraps the retained `pci_function_record_t` into a **PCI-facing device object**.

It should define something like:

* `pci_device_t`
* embedded base `device_t`
* BDF
* vendor/device ID
* class/subclass/prog-if
* pointers or copies of BAR metadata
* capability metadata
* IRQ metadata
* helper accessors for BAR lookup and capability lookup

This is where your current PCI registry becomes useful to later code. The PCI core should not expose raw registry internals to drivers. It should present a clean `pci_device_t` that is derived from retained registry state.

### 4.3 — PCI driver object and match rules

This subphase defines what a PCI driver is.

It should introduce something like:

* `pci_driver_t`
* driver name
* match strategy
* `probe(pci_device_t *pdev)`
* optional `remove(pci_device_t *pdev)`

For matching, start with:

* exact vendor/device match
* optional class/subclass/prog-if match later

Do not overbuild this. Phase 4 needs only enough matching power to bind one real PCI driver cleanly. The driver object is the contract between the PCI bus core and future drivers.

### 4.4 — Driver registry and bind/probe flow

This is the core of Phase 4.

You need a central registration path where PCI drivers register themselves, and a PCI bus-side probe path that:

* iterates retained PCI functions
* materializes a `pci_device_t`
* checks all registered PCI drivers for a match
* binds the first valid match
* invokes that driver’s `probe()`
* records success / failure in the device state

This is the first time the PCI subsystem becomes an actual **bus core** instead of only a discovery module.

The first version should use a simple fixed array for registered drivers and a simple first-match policy. That is enough.

### 4.5 — PCI driver helper API

Once `probe()` exists, the driver needs a minimal API surface so it does not reopen low-level PCI machinery itself.

This helper layer should include:

* enable device memory space / I/O space / bus mastering
* find BAR by index
* find first BAR of kind mem32/io
* find capability by kind
* claim or reference IRQ line metadata
* expose MMIO base / I/O base cleanly

This is important because if Phase 4 does not provide these helpers, your first real driver in Phase 5 will start reaching back into registry internals and raw PCI functions, which defeats the whole purpose of the device model.

### 4.6 — Validation with a dummy PCI driver

Before VirtIO, add one **minimal no-op or inspection driver** that binds to one known device in your QEMU bus, such as the Intel NIC or VGA device.

That test driver should:

* register with the PCI core
* match one known function
* print that it bound successfully
* inspect BARs/capabilities through the new helper API
* return success from `probe()`
