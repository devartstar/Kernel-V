# Kernel-V

**Kernel-V** is a custom x86 kernel built from scratch as a long-term systems learning, research, and experimentation project.

The project starts from the classical UNIX kernel model described in *The Design of the UNIX Operating System* by Maurice J. Bach, but its goal is not to stop at building a UNIX clone. Kernel-V uses UNIX-like fundamentals as the stable foundation, then grows into a **self-explaining, storage-aware, hardware-transparent research kernel**.

The long-term identity of Kernel-V is:

> **A minimal UNIX-like kernel designed for causal observability, hardware transparency, and storage-aware scheduling.**

---

## 1. Project Vision

Kernel-V is built to answer two questions at the same time:

1. **How does a UNIX-like kernel really work internally?**
2. **Can a kernel explain its own behavior clearly enough to make debugging, learning, and performance diagnosis radically easier?**

The project is intentionally built component by component:

- boot handoff
- kernel runtime
- memory management
- interrupts and syscalls
- processes and scheduling
- PCI and hardware discovery
- driver model
- block I/O
- file system
- userland
- IPC
- networking
- tracing
- resource accounting
- storage-aware scheduling
- deterministic debugging

The foundation is UNIX-like because UNIX gives clean, time-tested abstractions:

- files
- processes
- file descriptors
- pipes
- syscalls
- device files
- scheduler
- buffer cache
- process control
- IPC

The research direction is Kernel-V-specific:

- causal kernel tracing
- self-explaining runtime behavior
- per-process resource truth ledger
- storage-aware I/O control
- deterministic replay
- hardware and driver introspection
- educational TraceOS layer

---

## 2. Core Philosophy

Kernel-V follows a strict engineering philosophy:

### Build the primitive before abstracting it

Each subsystem begins as a minimal working implementation, then becomes cleaner and more general only after its behavior is understood.

### Keep retained state instead of repeatedly rediscovering facts

For example, the PCI subsystem stores discovered devices, BARs, command/status state, and capabilities in a retained registry instead of constantly rereading raw configuration space.

### Make every subsystem inspectable

Kernel-V should not hide state. Every important object should eventually be dumpable:

- process
- page table
- frame allocator
- syscall table
- scheduler queue
- PCI device
- driver binding
- block request
- file descriptor
- inode
- trace event

### Prefer correctness and clarity before performance

A slow kernel that is correct and explainable is better than a fast kernel that cannot be trusted.

### Use UNIX as the foundation, not the final destination

Kernel-V should implement the basic pillars of UNIX-like systems, but its unique value must come from observability, storage intelligence, and causal debugging.

---

## 3. Repository Structure

Recommended root structure:

```text
Kernel-V/
├── bootloader/              # External Stage-1 / Stage-2 bootloader integration
├── kernel/                  # Main kernel source tree
│   ├── arch/                # Architecture-specific code
│   │   └── x86/             # x86 protected-mode code, GDT, IDT, paging, traps
│   ├── include/             # Public kernel headers
│   ├── core/                # Kernel main, panic, printk, init flow
│   ├── mm/                  # Physical memory, paging, heap, VM
│   ├── proc/                # Process model, scheduler, context switch
│   ├── syscall/             # Syscall ABI and syscall dispatch
│   ├── drivers/             # Device drivers
│   ├── bus/                 # PCI and future bus subsystems
│   ├── fs/                  # VFS, inodes, file descriptors, file systems
│   ├── block/               # Block device and buffer cache layer
│   ├── ipc/                 # Pipes, signals, wait/wakeup, shared memory
│   ├── net/                 # Network stack
│   ├── trace/               # TraceOS event bus and causal tracing
│   └── libk/                # Kernel utility library
├── user/                    # Userland programs and minimal libc
│   ├── libc/                # Minimal syscall wrappers and C runtime
│   ├── init/                # First user process
│   └── bin/                 # Shell and user commands
├── learning/                # Isolated mini-projects and experiments
├── docs/                    # Architecture notes, roadmap, diagrams, design docs
├── tests/                   # Boot-time and subsystem tests
├── tools/                   # Disk image builders, filesystem tools, trace tools
├── scripts/                 # Build, run, debug, and QEMU scripts
├── Makefile                 # Top-level build entry
└── README.md
```

---

## 4. Learning Folder Model

The `learning/` folder is used for isolated experiments before concepts are integrated into the main kernel.

Example structure:

```text
learning/
├── 00-hello-boot/
├── 01-vga-print/
├── 02-gdt-protected-mode/
├── 03-idt-exceptions/
├── 04-paging-basics/
├── 05-frame-allocator/
├── 06-user-mode-switch/
├── 07-syscall-int80/
├── 08-context-switch/
├── 09-pci-config-space/
├── 10-pci-bar-decode/
├── 11-buffer-cache-prototype/
├── 12-inode-model/
└── README.md
```

Each mini-project should contain:

```text
README.md
src/
Makefile
notes.md
expected-output.txt
```

Each mini-project README should answer:

- What concept is being tested?
- Why does this concept matter in the main kernel?
- What code was written?
- What output proves correctness?
- What bugs or edge cases were discovered?
- What part of Kernel-V will eventually use this?

---

## 5. Current Project Baseline

Kernel-V has already progressed beyond a simple boot demo.

Completed or substantially implemented foundations:

- custom Stage-1 and Stage-2 bootloader in a separate bootloader project
- protected-mode kernel entry
- VGA output and kernel logging
- `printk`-style logging infrastructure
- panic and debug output foundations
- paging fundamentals
- CR3 switching
- user/kernel transition
- syscall path
- user process execution
- process scheduling experiments
- process cleanup path
- multiple user-process tests
- PCI configuration-space access
- PCI bus-0 enumeration
- PCI retained registry
- PCI Type-0 BAR decoding
- PCI command/status tracking
- PCI capability-list walking in progress

The active engineering area is currently:

```text
PCI Phase 3 closure -> Device/Driver Model -> First real driver binding
```

---

## 6. Roadmap Overview

```text
Phase 0   Bootloader handoff contract
Phase 1   Kernel runtime, logging, panic, and tests
Phase 2   Memory management and paging
Phase 3   Interrupts, exceptions, and syscalls
Phase 4   Processes, scheduler, user mode, and cleanup
Phase 5   PCI discovery and retained hardware registry
Phase 6   Kernel device/driver model
Phase 7   Block I/O and buffer cache
Phase 8   VFS, inode model, file descriptors, RAMFS/KVFS
Phase 9   ELF loader, libc, init, and shell
Phase 10  IPC: pipes, signals, wait/wakeup
Phase 11  Networking: e1000, Ethernet, ARP, IPv4, UDP
Phase 12  TraceOS: event bus, timeline, causal debugging
Phase 13  Resource ledger: per-process CPU/memory/I/O truth
Phase 14  Storage-aware scheduler and I/O control
Phase 15  Deterministic replay and kernel doctor
Phase 16  Capability-style security model
```

---

# 7. Detailed Roadmap

---

## Phase 0 — Bootloader Handoff Contract

**Status:** external bootloader mostly ready.

Kernel-V should not duplicate bootloader work. The bootloader should provide a clean handoff contract to the kernel.

### Goals

- Define a stable boot information structure.
- Pass memory map information.
- Pass kernel load address and size.
- Pass boot flags.
- Eventually support ELF loading.

### Example boot handoff structure

```c
typedef struct kv_boot_info {
    uint32_t magic;
    uint32_t bootloader_version;
    uint32_t kernel_load_phys;
    uint32_t kernel_size;
    uint32_t e820_map_phys;
    uint32_t e820_entry_count;
    uint32_t framebuffer_or_vga_mode;
    uint32_t boot_flags;
} kv_boot_info_t;
```

### Deliverable

```text
Stage2 loads Kernel-V -> jumps to kernel_entry(boot_info*)
```

---

## Phase 1 — Kernel Runtime

**Status:** mostly completed.

### Goals

- Kernel entry.
- Linker script.
- VGA and serial logging.
- `printk`-style logging.
- Panic path.
- Assertion support.
- Boot-time test mode.

### Key outputs

- `kernel_main()`
- `printk`
- `KLOG_INFO`
- `KLOG_ERROR`
- `panic`
- `ASSERT`
- boot-time test runner

### Deliverable

A bootable kernel that can print logs, fail loudly, and execute test-mode validations.

---

## Phase 2 — Memory Management and Paging

**Status:** substantially progressed.

### Goals

- Consume physical memory map.
- Implement frame allocator.
- Implement paging.
- Maintain kernel/user memory split.
- Add kernel heap.
- Add safe user-memory access helpers.

### Core APIs

```c
paddr_t frame_alloc(void);
void frame_free(paddr_t frame);

int paging_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags);
int paging_unmap_page(uintptr_t virt);

int copy_from_user(void *kdst, const void *usrc, size_t n);
int copy_to_user(void *udst, const void *ksrc, size_t n);
bool user_range_valid(uintptr_t uaddr, size_t len, uint32_t access);
```

### Important concepts

- physical address
- virtual address
- page directory
- page table
- PDE
- PTE
- CR3
- TLB flush
- identity mapping
- higher-half kernel
- user/supervisor page permissions

### Deliverable

Kernel can allocate physical frames, map virtual memory, switch address spaces, and safely validate user pointers.

---

## Phase 3 — Interrupts, Exceptions, and Syscalls

**Status:** mostly implemented for current needs.

### Goals

- GDT and privilege levels.
- IDT.
- CPU exception handlers.
- PIC remapping.
- PIT timer.
- Keyboard IRQ.
- Page fault handler.
- `int 0x80` syscall ABI.
- Syscall dispatch table.

### Syscall ABI

```text
eax = syscall_number
ebx = arg1
ecx = arg2
edx = arg3
esi = arg4
edi = arg5
ebp = arg6
```

### Required exception handlers

```text
#DE  divide error
#UD  invalid opcode
#GP  general protection fault
#PF  page fault
```

### Deliverable

User programs can enter the kernel through a stable syscall ABI, and kernel exceptions produce useful diagnostics.

---

## Phase 4 — Processes, Scheduler, and User Mode

**Status:** strong progress.

### Goals

- Process object.
- Kernel stack per process.
- User stack setup.
- Context switching.
- CR3 switching.
- User-mode entry using `iret`.
- Round-robin scheduler.
- Process cleanup.
- Multi-user-process tests.

### Core process object

```c
typedef struct proc {
    uint32_t pid;
    uint32_t state;
    uint32_t *page_dir;
    uintptr_t kstack_top;
    uintptr_t user_entry;
    uintptr_t user_stack_top;
    struct proc *parent;
} proc_t;
```

### Process states

```text
NEW
RUNNABLE
RUNNING
BLOCKED
ZOMBIE
DEAD
```

### Deliverable

Kernel can create, run, switch, and clean up user processes.

---

## Phase 5 — PCI Discovery and Hardware Registry

**Status:** active phase.

Kernel-V is currently building a retained PCI subsystem.

### Goals

- PCI config-space read/write.
- Bus-0 enumeration.
- Device/function identity decode.
- Type-0 BAR decoding.
- Command/status state tracking.
- PCI capability-list walking.
- Retained PCI registry.
- Unified phase dump.

### Retained state layers

Each PCI function should retain:

```text
identity
BAR/resource state
command/status state
capability-list state
```

### Correct Phase 3 PCI dump order

```text
identity line
command/status line
BAR/resource lines
capability summary
per-capability lines
```

### Important design rule

The PCI dump must use retained registry state only. It must not secretly reread config space while dumping.

### Phase 5 closure checklist

```text
pci_dump_record_full()
pci_dump_registry_phase3()
pci_phase3_validate()
pci_phase3_validate_test()
phase3 contract header
```

### Deliverable

PCI subsystem can discover hardware and provide rich retained device records to the future driver model.

---

## Phase 6 — Kernel Device and Driver Model

**Status:** next major phase.

Do not jump directly from PCI enumeration into a specific device driver. First build the kernel driver model.

### Goals

- Generic device object.
- PCI device wrapper.
- Driver object.
- Match/probe/remove lifecycle.
- Driver registration.
- Device binding state.
- First real driver.

### Generic device object

```c
typedef struct kv_device {
    const char *name;
    uint32_t type;
    void *bus_private;
    void *driver_private;
} kv_device_t;
```

### PCI device wrapper

```c
typedef struct pci_device {
    kv_device_t dev;
    pci_function_record_t *pci;
} pci_device_t;
```

### PCI driver object

```c
typedef struct pci_driver {
    const char *name;
    bool (*match)(const pci_function_record_t *rec);
    int  (*probe)(pci_device_t *dev);
    void (*remove)(pci_device_t *dev);
} pci_driver_t;
```

### Binding lifecycle

```text
DISCOVERED
MATCHED
PROBING
BOUND
FAILED
REMOVED
```

### Deliverable

Kernel supports structured device-driver binding instead of hardcoded driver initialization.

---

## Phase 7 — Block I/O and Buffer Cache

**Status:** future.

This phase connects the hardware track with the UNIX file-system model.

### Goals

- Block device abstraction.
- RAM disk.
- Block request object.
- Buffer cache.
- Dirty buffer tracking.
- Basic read/write path.
- Later: ATA, AHCI, virtio-blk, or NVMe.

### Block device interface

```c
typedef struct block_device {
    uint64_t sector_count;
    uint32_t sector_size;
    int (*read)(struct block_device *, uint64_t lba, void *buf, uint32_t count);
    int (*write)(struct block_device *, uint64_t lba, const void *buf, uint32_t count);
} block_device_t;
```

### Deliverable

Kernel can read and write logical blocks through a stable block layer.

---

## Phase 8 — VFS, Inodes, and File Descriptors

**Status:** future.

This is the major UNIX file-system phase.

### Goals

- VFS abstraction.
- In-memory inode model.
- Superblock.
- File object.
- Per-process file descriptor table.
- Pathname lookup.
- Directory operations.
- RAMFS first.
- KVFS or ext2 reader later.

### Core objects

```c
struct vnode;
struct inode;
struct superblock;
struct file;
struct dentry;
```

### File syscalls

```text
open
read
write
close
lseek
stat
dup
mkdir
unlink
```

### Deliverable

User programs can access files through file descriptors.

---

## Phase 9 — Userland, ELF Loader, libc, and Shell

**Status:** partial; future expansion needed.

### Goals

- Load real user binaries.
- Support ELF eventually.
- Build minimal libc.
- Add `/init`.
- Add a simple shell.
- Add user commands.

### Minimal libc wrappers

```c
int write(int fd, const void *buf, unsigned len);
int read(int fd, void *buf, unsigned len);
int open(const char *path, int flags);
int close(int fd);
void exit(int code);
```

### Shell commands

```text
echo
cat
ls
run
ps
mem
pci
trace
doctor
```

### Deliverable

Kernel boots into userland and runs useful programs.

---

## Phase 10 — IPC and UNIX Process Semantics

**Status:** future.

### Goals

- Pipes.
- Blocking read/write.
- Wait queues.
- Signals.
- `wait`.
- `exit`.
- `kill`.
- Shared memory later.

### Important syscalls

```text
pipe
read
write
close
fork
exec
wait
exit
kill
signal
```

### Deliverable

Processes can communicate, synchronize, terminate, and notify each other.

---

## Phase 11 — Networking

**Status:** future.

### Goals

- e1000 or virtio-net driver.
- Packet buffer.
- Ethernet frame parsing.
- ARP.
- IPv4.
- ICMP ping.
- UDP.
- TCP later.

### Packet buffer

```c
typedef struct mbuf {
    uint8_t *data;
    uint32_t len;
    struct mbuf *next;
} mbuf_t;
```

### Deliverable

Kernel can send and receive packets and eventually expose a simple socket-like interface.

---

## Phase 12 — TraceOS: Self-Explaining Kernel Layer

**Status:** future but should begin soon in small form.

TraceOS is the unique observability layer of Kernel-V.

### Core idea

Every important kernel action emits a structured event.

### Event examples

```text
process created
context switch
page mapped
page unmapped
page fault
syscall enter
syscall exit
PCI device discovered
driver probe started
driver probe failed
block request submitted
interrupt received
```

### Minimal API

```c
typedef enum kv_event_type {
    KV_EVENT_SYSCALL_ENTER,
    KV_EVENT_SYSCALL_EXIT,
    KV_EVENT_CONTEXT_SWITCH,
    KV_EVENT_PAGE_FAULT,
    KV_EVENT_PCI_DISCOVER,
    KV_EVENT_DRIVER_PROBE,
    KV_EVENT_BLOCK_IO,
} kv_event_type_t;

void kv_trace_emit(kv_event_type_t type,
                   uint32_t subject_id,
                   uint32_t arg0,
                   uint32_t arg1,
                   uint32_t arg2);
```

### Shell commands

```text
trace on
trace off
trace dump
trace sched
trace pci
trace faults
```

### Deliverable

Kernel-V becomes observable from inside itself.

---

## Phase 13 — Per-Process Resource Truth Ledger

**Status:** future research track.

### Core idea

Each process gets a live resource ledger.

### Metrics

```text
CPU time
syscall count
page faults
heap growth
file reads
file writes
block I/O issued
bytes dirtied
interrupts caused
scheduler delay
device wait time
```

### Example command

```sh
ledger pid 5
```

### Example output

```text
pid=5 /bin/copy
cpu=12ms
syscalls=302
read_bytes=4096
write_bytes=4096
page_faults=3
blocked_on=block_device:ram0
storage_cost=8 sectors
scheduler_wait=4 ticks
```

### Deliverable

Kernel can explain what each process costs the system.

---

## Phase 14 — Storage-Aware Scheduling and I/O Control

**Status:** future research track.

This is the strongest practical research direction for Kernel-V.

### Core idea

Make storage behavior visible and controllable per process.

### Goals

- Per-process I/O accounting.
- Block latency tracking.
- Queue depth tracking.
- Dirty-page lifetime tracking.
- Storage fairness.
- IOPS throttling.
- Bandwidth throttling.
- Storage pressure diagnosis.

### Commands

```text
storage top
storage trace pid 4
storage heatmap
storage who-wrote-block 18320
storage throttle pid 7 --iops 100
```

### Deliverable

Kernel-V becomes a research kernel for storage-aware scheduling and per-process I/O accountability.

---

## Phase 15 — Deterministic Replay and Kernel Doctor

**Status:** future research track.

### Deterministic replay

The kernel should eventually replay selected event streams:

```text
fixed timer ticks
scripted interrupts
scripted scheduler decisions
scripted device responses
trace replay
```

### Kernel doctor

Command:

```sh
doctor
```

Example output:

```text
System diagnosis:
- 2 processes runnable
- 1 process blocked on block I/O
- pid=4 has 128 page faults in the last 5 seconds
- pci:e1000 interrupt rate is high
- block queue latency increased from 2 ticks to 19 ticks
Likely bottleneck: block I/O wait
```

### Deliverable

Kernel-V can diagnose performance and correctness problems in human-readable form.

---

## Phase 16 — Capability-Style Security

**Status:** future research track.

### Core idea

Keep UNIX simplicity, but reduce ambient authority.

### Capabilities

```text
cap_file_read
cap_file_write
cap_net_send
cap_net_recv
cap_device_mmio
cap_trace_read
```

### Example

```sh
spawn /bin/netdemo --cap net_send --cap net_recv
spawn /bin/editor --cap file_read:/docs --cap file_write:/docs
```

### Deliverable

Kernel-V explores a cleaner security model while preserving UNIX-like usability.

---

# 8. What Makes Kernel-V Different?

Kernel-V is not intended to become a Linux replacement.

Kernel-V is not intended to be only a UNIX clone.

Kernel-V is intended to become:

```text
A small, inspectable, UNIX-like research kernel that explains itself.
```

The unique features are:

## 8.1 Self-explaining kernel behavior

Instead of only printing:

```text
page fault at 0x00402000
```

Kernel-V should eventually explain:

```text
Process 4 faulted at 0x00402000.
Reason: present=0, write=1, user=1.
The address belongs to an unmapped user page.
Last related mapping event:
  ELF loader mapped .data at 0x00401000-0x00402000.
No mapping exists for 0x00402000.
Likely cause: invalid pointer or stack overflow.
```

## 8.2 Causal debugging

Kernel-V should track not only what happened, but what caused it.

Example event structure:

```c
typedef struct kv_trace_event {
    uint64_t event_id;
    uint64_t parent_event_id;
    uint32_t type;
    uint32_t subject_id;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t arg2;
    uint64_t timestamp;
} kv_trace_event_t;
```

## 8.3 Hardware transparency

PCI and drivers should be explainable:

```text
pci explain-bar 00:03.0 0
pci explain-cap 00:03.0 msi
driver why-unbound 00:03.0
```

Example:

```text
BAR0 is MMIO.
Raw value: 0xfebc0000.
Memory decoding is disabled.
Driver e1000 matched this device but probe failed.
Reason: bus mastering is not enabled.
Suggested next action: enable PCI command bit 2.
```

## 8.4 Storage intelligence

Kernel-V should eventually answer:

```text
Which process is causing I/O pressure?
Which file caused dirty-page growth?
Which block device is the bottleneck?
Which process should be throttled?
Why is this process blocked?
```

## 8.5 Kernel as a live textbook

The kernel should be able to explain runtime flows:

```text
explain syscall write
explain pagefault 0x400020
explain scheduler
explain inode /hello.txt
explain pci 00:03.0
```

---

# 9. Development Method

Each major subsystem follows this sequence:

```text
1. Build a small isolated learning project.
2. Write notes and expected output.
3. Integrate the concept into the main kernel.
4. Add boot-time tests.
5. Add logs.
6. Add trace events.
7. Add documentation.
8. Add a phase contract.
```

A phase is not complete until it has:

- working code
- test coverage
- kernel logs
- dump/debug command or function
- documented limitations
- next-phase boundary

---

# 10. Phase Contract Style

Each phase should end with a contract file.

Example:

```text
docs/contracts/pci-phase3-contract.md
```

Example content:

```text
Supported:
- bus 0 PCI enumeration
- retained PCI function registry
- Type-0 BAR decode
- command/status snapshots
- conventional PCI capability walking
- unified PCI dump

Not yet supported:
- recursive bridge discovery
- BAR sizing
- MSI/MSI-X enablement
- PCIe extended capabilities
- power management
- driver binding
```

This keeps the project honest and prevents vague progress.

---

# 11. Immediate Next Steps

The next concrete engineering sequence is:

```text
1. Complete PCI Phase 3 closure.
2. Add unified retained PCI dump.
3. Add PCI Phase 3 validator.
4. Add PCI Phase 3 contract document.
5. Start Phase 6: Kernel Device/Driver Model.
6. Implement generic device object.
7. Implement PCI driver object.
8. Implement match/probe/remove flow.
9. Bind the first real driver.
10. Add TraceOS event emission to PCI discovery and driver probing.
```

The first unique feature should be small:

```c
kv_trace_emit(event_type, subject_id, arg0, arg1, arg2);
```

Add it first to:

```text
syscall enter/exit
context switch
page fault
PCI discovery
driver probe
```

That single step begins the transition from UNIX clone to observable research kernel.

---

# 12. Reference Books

Primary conceptual reference:

```text
The Design of the UNIX Operating System
Maurice J. Bach
```

Useful programmer-facing UNIX reference:

```text
Advanced Programming in the UNIX Environment
W. Richard Stevens, Stephen A. Rago
```

Additional implementation references:

```text
xv6
MINIX
Linux 0.11
OSDev Wiki
Intel Software Developer Manuals
PCI Local Bus Specification
VirtIO Specification
```

---

# 13. Final Project Identity

Kernel-V should be described as:

```text
Kernel-V is a minimal UNIX-like x86 kernel built from scratch to study operating
system internals deeply, while evolving into a self-explaining research kernel focused
on causal observability, hardware transparency, and storage-aware scheduling.
```

The short version:

```text
Kernel-V: a UNIX-like kernel that explains itself.
```
