# Kernel-V DSSI — Document 00: Overview & North Star

> **Distributed Single System Image (DSSI) for Kernel-V**
> The transformation of Kernel-V from a single-node UNIX-like kernel into a fabric of cooperating kernels that present themselves to user programs as **one infinitely large machine**.

This is the first of eight documents that together describe how Kernel-V grows from its current single-node state (Phase 5) into a fully distributed system-image kernel where any process can run on any node, transparently.

---

## Table of Contents

1. [The Real-World Problem](#1-the-real-world-problem)
2. [The Vision in One Sentence](#2-the-vision-in-one-sentence)
3. [The Mental Model](#3-the-mental-model)
4. [Architecture at 30,000 Feet](#4-architecture-at-30000-feet)
5. [Full Phase Map (Phases 6 → 28)](#5-full-phase-map-phases-6--28)
6. [The Eight-Document Plan](#6-the-eight-document-plan)
7. [Prior Art: What Has Been Tried](#7-prior-art-what-has-been-tried)
8. [Why Now: What Modern Hardware Changes](#8-why-now-what-modern-hardware-changes)
9. [Design Principles for Kernel-V DSSI](#9-design-principles-for-kernel-v-dssi)
10. [Glossary of Terms](#10-glossary-of-terms)
11. [Success Criteria & Demo Milestones](#11-success-criteria--demo-milestones)
12. [Risks, Tradeoffs, and What We Will NOT Build](#12-risks-tradeoffs-and-what-we-will-not-build)
13. [Reading Map](#13-reading-map)

---

## 1. The Real-World Problem

Every computer user — student, gamer, developer, scientist — has experienced this:

> *"My machine is hung. My CPU is at 100%. My fan is screaming. Meanwhile, my friend's laptop on the same Wi-Fi is sitting idle at 5% CPU, doing nothing."*

The resources are **right there**. Across the room. On the same network. But there is no way to use them.

This is the **finite local resource problem**, and it manifests in many forms:

| Symptom | Underlying Cause |
|---------|-----------------|
| Compile takes 40 minutes on laptop, would take 5 on desktop | CPU silo |
| Browser tab eats all 8GB of RAM, system swaps | Memory silo |
| Video render pegs single machine, others are idle | GPU/CPU silo |
| Cannot run large simulation because local RAM is too small | Memory ceiling |
| Battery dies during heavy work; plugged-in machine sits idle | Power asymmetry |
| ML training fits on no single available machine | Aggregation impossible |

The cloud "solves" this — but the cloud is:
- **Owned by someone else** (privacy, cost, lock-in)
- **Distant** (latency)
- **Coarse-grained** (you rent a whole VM, not a few seconds of CPU)
- **Opaque** (you do not see what's happening)

What if instead, **every kernel on your local network became a contributor to a shared resource pool**, and your processes could automatically flow to whichever machine had the most capacity?

That is what Kernel-V DSSI is about.

---

## 2. The Vision in One Sentence

> **Kernel-V DSSI is a fabric of cooperating Kernel-V instances that, together, present to every user process the illusion of a single computer with the combined CPU, memory, and I/O capacity of every node in the fabric.**

A process started on your laptop may finish on your desktop. A process that needs 32 GB of RAM may run on a node with only 8 GB locally — the rest is faulted in from peer nodes. A process that wants to read a file does not need to know whether the file lives on this disk or a disk three nodes away.

The user does not think about migration. The user does not think about which machine ran what. The user thinks about **one machine** — even though, physically, it is many.

---

## 3. The Mental Model

The clearest way to think about this is in three layers:

```
┌──────────────────────────────────────────────────────────┐
│                     USER PROCESSES                         │
│            (think they run on one big machine)             │
├──────────────────────────────────────────────────────────┤
│                                                            │
│             FABRIC LAYER  (the "management layer")         │
│   ┌──────────────────────────────────────────────────┐    │
│   │  Resource Monitor   │   Peer Registry            │    │
│   │  Migration Engine   │   Failure Detector         │    │
│   │  Syscall Proxy      │   Distributed Scheduler    │    │
│   │  Checkpoint/Restore │   Trust & Authentication   │    │
│   └──────────────────────────────────────────────────┘    │
│                                                            │
├──────────────────────────────────────────────────────────┤
│             LOCAL KERNEL  (Kernel-V single node)           │
│      processes, memory, scheduler, drivers, I/O, FS        │
└──────────────────────────────────────────────────────────┘
                            ↕
                  ════════ NETWORK ════════
                            ↕
              [ same three layers on every peer ]
```

The **Fabric Layer** is what you (the user) called the "management layer." It sits between the user-visible interface and the raw kernel. It has two distinct sub-planes:

- **Control Plane** — Who is on the fabric? Who has free CPU? Who just went offline? Where should this process run? *(Low bandwidth, high frequency, eventually consistent.)*
- **Data Plane** — Actually move the process's memory pages, registers, file descriptors, and stack to a remote node. *(High bandwidth, latency-critical, requires correctness.)*

Most of the engineering effort is in the data plane. Most of the *design* effort is in the control plane.

---

## 4. Architecture at 30,000 Feet

Here is the full picture of a 4-node Kernel-V fabric, with one process being live-migrated from Node A to Node C:

```
       NODE A (your laptop)              NODE C (idle desktop)
   ┌──────────────────────────┐       ┌──────────────────────────┐
   │  proc 12 (running...)    │       │  proc 12 (resuming...)   │
   │  ┌────────────────────┐  │       │  ┌────────────────────┐  │
   │  │ kernel stack       │──┼─────► │  │ kernel stack       │  │
   │  │ user pages 0-127   │──┼─────► │  │ user pages 0-127   │  │
   │  │ registers          │──┼─────► │  │ registers          │  │
   │  │ file descriptors   │──┼──┐    │  │ FD redirected ─────┼─┐│
   │  └────────────────────┘  │  │    │  └────────────────────┘ ││
   │  Fabric: tracking dirty  │  │    │  Fabric: receiving      ││
   │   pages, sending deltas  │  │    │   pages, building proc  ││
   └──────────────────────────┘  │    └──────────────────────────┘│
              ▲                  │                ▲               │
              │   syscall        │                │   read(fd=3)  │
              │   proxy          └────────────────┴───────────────┘
              │   (file lives on Node A's disk)
              │
   ┌──────────┴───────────────┐       ┌──────────────────────────┐
   │  NODE B (server)         │       │  NODE D (mobile, slow)   │
   │  fabric peer             │       │  fabric peer             │
   │  contributing storage    │       │  read-only participant   │
   └──────────────────────────┘       └──────────────────────────┘

       ═══════════ Fabric Control Plane (gossip) ═══════════
       (every node knows every node's CPU%, RAM free, load)
```

The user on Node A typed `./render_video`. The fabric noticed Node C was idle. Mid-execution, the process was transferred. When it tries to read its input file from `/home/user/movie.mp4`, the file descriptor is still tied to Node A — so the read syscall is proxied back. The user sees nothing different except that their fan stopped spinning.

---

## 5. Full Phase Map (Phases 6 → 28)

This is the complete journey from where you are today (end of Phase 5) to full DSSI. The first six phases (6-11) are already in your existing `kernel_plan.md` — they remain mostly unchanged, but with DSSI seeds woven in (covered in Document 01).

```
═══════════════ PART I: FOUNDATION (your existing plan) ═══════════════

Phase 6   Kernel Device/Driver Model            [from kernel_plan.md]
Phase 7   Block I/O and Buffer Cache            [from kernel_plan.md]
Phase 8   VFS, Inodes, File Descriptors         [from kernel_plan.md]
Phase 9   ELF Loader, libc, init, Shell         [from kernel_plan.md]
Phase 10  IPC: pipes, signals, wait/wakeup      [from kernel_plan.md]
Phase 11  Networking: e1000, Eth, ARP, IPv4     [from kernel_plan.md]

═══════════════ PART II: OBSERVABILITY (prereq for fabric) ═══════════

Phase 12  TraceOS: event bus, causal debug      [DSSI-extended]
Phase 13  Per-Process Resource Truth Ledger     [DSSI-extended]

═══════════════ PART III: CHECKPOINT/RESTORE (local first) ═══════════

Phase 14  Kernel Heap & Slab Allocator          [DSSI prereq]
Phase 15  Process Checkpoint / Restore (local)  [DSSI core primitive]

═══════════════ PART IV: FABRIC CONTROL PLANE ═══════════════════════

Phase 16  Fabric Identity & Node Bootstrap
Phase 17  Gossip Protocol & Resource Advertisement

═══════════════ PART V: FIRST REMOTE EXECUTION ═══════════════════════

Phase 18  Stop-and-Copy Process Migration
Phase 19  Syscall Proxy / Home-Node Forwarding

═══════════════ PART VI: LIVE MIGRATION ═════════════════════════════

Phase 20  Pre-Copy Live Migration
Phase 21  Post-Copy Migration & On-Demand Paging
Phase 22  Hybrid Migration & Compression

═══════════════ PART VII: TRANSPARENT FABRIC ═══════════════════════

Phase 23  Distributed Fabric Scheduler
Phase 24  Distributed VFS & File Locality

═══════════════ PART VIII: ADVANCED & RESEARCH ═════════════════════

Phase 25  RDMA-Class Transport (zero-copy networking)
Phase 26  Disaggregated Memory (CXL-inspired)
Phase 27  Security, Trust, and Encrypted Migration
Phase 28  Global Mesh & Internet-Scale Fabric
```

**Estimated timeline (1-2 years of dedicated work):**

| Block | Phases | Estimated Time | Cumulative |
|-------|--------|---------------|-----------|
| Foundation (Part I) | 6-11 | 4-6 months | 6 mo |
| Observability (Part II) | 12-13 | 1-2 months | 8 mo |
| Checkpoint primitive (Part III) | 14-15 | 2 months | 10 mo |
| Control plane (Part IV) | 16-17 | 1-2 months | 12 mo |
| **First demo!** (Part V) | 18-19 | 2-3 months | **15 mo** |
| Live migration (Part VI) | 20-22 | 3-4 months | 19 mo |
| Transparent fabric (Part VII) | 23-24 | 2-3 months | 22 mo |
| Advanced (Part VIII) | 25-28 | Open-ended | 24 mo+ |

The **first end-to-end demo of process migration on real hardware** lands around month 15 — roughly the midpoint of your two-year window. Everything after that is refinement and research.

---

## 6. The Eight-Document Plan

This overview is Document 00. The remaining seven documents each go deep on one chunk of the journey:

| # | Document | Covers Phases | Purpose |
|---|----------|---------------|---------|
| **00** | `dssi_00_overview.md` | All | This document. The north star. |
| **01** | `dssi_01_foundation_seeds.md` | 6-11 | DSSI-aware additions to your existing foundation phases. Things to build *now* that pay dividends later. |
| **02** | `dssi_02_observability.md` | 12-13 | TraceOS and Resource Ledger from the fabric's perspective. |
| **03** | `dssi_03_checkpoint_restore.md` | 14-15 | The single hardest primitive. Built and tested locally before any networking. |
| **04** | `dssi_04_fabric_control_plane.md` | 16-17 | Node identity, gossip, failure detection, resource advertisement. |
| **05** | `dssi_05_first_remote_exec.md` | 18-19 | Stop-and-copy + syscall proxy. Your first working demo. |
| **06** | `dssi_06_live_migration.md` | 20-22 | Pre-copy, post-copy, hybrid, compression. |
| **07** | `dssi_07_advanced.md` | 23-28 | Distributed scheduler, distributed VFS, RDMA, disaggregated memory, security, internet-scale. |

Each document follows the same structure:
- **Purpose** — what this phase achieves
- **Prerequisites** — what must exist first
- **Detailed sub-phases** — broken into bite-sized milestones
- **Core data structures** — concrete C types
- **APIs** — function signatures with semantics
- **Technologies & techniques** — cutting-edge approaches to consider
- **Common pitfalls** — what to watch out for
- **Testing strategy** — how to validate
- **Deliverable** — what "done" looks like

---

## 7. Prior Art: What Has Been Tried

You are not the first to attempt this. Standing on the shoulders of giants saves years.

### 7.1 MOSIX (1977-2011, Hebrew University)

The pioneer. MOSIX added process migration to BSD and later Linux. Its key insight: **separate the process into a "user context" that can migrate and a "system context" that stays on the home node**. File and socket syscalls on the migrated process were transparently forwarded back to the home node — a technique called **deputizing** or **home-node forwarding**.

**What MOSIX got right:**
- Transparent process migration with no application changes
- Home-node syscall forwarding (simpler than distributed FS)
- Adaptive load-balancing based on observed behavior
- Migration cost estimation before deciding to migrate

**What MOSIX got wrong / what's outdated:**
- Pre-copy only, no post-copy (post-copy is much faster in many workloads)
- No compression of memory during transfer
- Heartbeat-based failure detection (Phi Accrual is now standard)
- No GPU/accelerator awareness
- Closed-source for much of its life — community could not extend it

**Read:** Barak & La'adan, *"The MOSIX multicomputer operating system for high performance cluster computing,"* Future Generation Computer Systems, 1998.

### 7.2 Kerrighed (2001-2010, INRIA / Kerlabs)

A full Single System Image cluster OS built on Linux. Provided process migration, distributed shared memory, distributed file system, and a unified `/proc` showing all nodes' processes as if they were local.

**What Kerrighed got right:**
- True SSI: `ps` showed processes from all nodes
- Distributed shared memory (DSM) — pages could exist on any node, coherence maintained
- File system unified across cluster

**What killed Kerrighed:**
- DSM coherence was the bottleneck — every shared write became a network round-trip
- Kept trying to track upstream Linux kernel changes; lost the race
- Required all nodes to run *identical* kernel versions
- Failure modes were catastrophic — losing one node could hang the whole cluster

**Read:** Morin et al., *"Kerrighed: a Single System Image Cluster Operating System for High Performance Computing,"* Euro-Par 2003.

### 7.3 Plan 9 from Bell Labs (1987+, ongoing)

Took a different angle: **everything is a file, and files can live on any machine accessible through a uniform network protocol (9P)**. Rather than migrate processes, Plan 9 exported resources as filesystems. Want to use a remote CPU? Mount its `/proc` and write to a file there.

**What Plan 9 got right:**
- Astonishingly clean primitives (9P protocol is ~10 message types)
- Per-process namespaces (each process can mount different things at different paths)
- Network transparency is *the* abstraction, not bolted on
- Still alive and inspiring (9front, Inferno, drawterm)

**What Plan 9 chose not to do:**
- Did not transparently migrate processes — applications had to opt in by running on a chosen CPU server
- Did not aggregate resources (you choose one CPU server; you don't get the union)

**Why it matters for Kernel-V:** The Plan 9 approach (export resources via files) is a brilliant *complement* to MOSIX's approach (migrate processes). You can do both. Plan 9's 9P protocol is a candidate inspiration for your control plane.

**Read:** Pike et al., *"Plan 9 from Bell Labs,"* Computing Systems, 1995.

### 7.4 openSSI (2001-2010, HP / community)

A Linux-based SSI for Beowulf clusters. Similar goals to Kerrighed, similar fate. Died of the same disease: trying to track upstream Linux.

### 7.5 LOCUS (1980s, UCLA)

The ancestor of all this. First system to provide transparent process migration. Hugely influential research.

### 7.6 More recent: Popcorn Linux (Virginia Tech, 2014-present)

Migrates processes between **heterogeneous ISAs** (e.g., x86 ↔ ARM). Compiles binaries for both architectures, migrates between them. Niche but technically impressive.

### 7.7 The cloud era: containers, not processes

Kubernetes, Nomad, etc. moved the abstraction up to **containers/pods**. This is *much* easier — a container is a self-contained unit, no need to track per-process state. But it loses the magic: your single program does not migrate; you must structure your app as many containers.

### 7.8 The forgotten lesson

All the SSI projects died in the 2010s. Why? Two reasons:
1. **Cloud ate the use case.** Why share your friend's laptop when AWS exists?
2. **Hardware was too slow.** 1 GbE made memory migration painful. Pre-copy needed 100+ MB/s sustained for tolerable downtime.

Both of those reasons have changed. (See next section.)

---

## 8. Why Now: What Modern Hardware Changes

The 2010s assumption that "distributed kernel migration is too slow" is no longer true. Here's what changed:

### 8.1 Networks got 100x faster

| Year | Common LAN | Common datacenter | Latency |
|------|-----------|-------------------|---------|
| 2005 | 100 Mbit/s | 1 GbE | 100 µs |
| 2015 | 1 GbE | 10 GbE | 30 µs |
| 2025 | 2.5 GbE / Wi-Fi 6E | 100 GbE / 400 GbE | 1-5 µs |

A 1 GB process working set transferred at 100 GbE is **80 ms**. At 400 GbE, **20 ms**. That's a blink. Pre-copy migration becomes practical for almost any workload.

### 8.2 RDMA went mainstream

**Remote Direct Memory Access (RDMA)** lets one machine read/write another machine's memory *without* involving the remote CPU. The hardware handles it. Latency drops to **single-digit microseconds**. This is the technology that makes post-copy migration (fault pages on demand from a remote machine) viable.

Two flavors:
- **InfiniBand** — purpose-built, expensive, datacenter only
- **RoCE (RDMA over Converged Ethernet)** — runs RDMA over standard Ethernet, requires lossless network (DCB/PFC) for v1; v2 is routable

For your fabric: RoCEv2 in datacenters, **soft-RoCE** (software RDMA) on commodity hardware as a fallback. There's even **rsockets** and **libfabric** which give a portable RDMA API.

### 8.3 CXL: Disaggregated memory becomes real

**Compute Express Link (CXL)** is a new interconnect (CXL 2.0 in 2025 hardware, CXL 3.0 coming) that lets multiple machines share a memory pool **as if it were local RAM**. The kernel can map remote memory into the process address space with cache-coherent semantics.

For Kernel-V DSSI: even without CXL hardware, the *concept* of disaggregated memory is a design north star. Your fabric can fake CXL semantics in software: a process can have pages physically on Node A but the page table on Node B traps and fetches them.

### 8.4 io_uring changed how kernels do I/O

Linux's `io_uring` (2019) introduced **shared ring buffers between userspace and kernel** for async I/O — no syscalls in the hot path, no copies, batched. The same idea applies to fabric communication: shared rings between kernel and NIC, between kernel and fabric peer.

For Kernel-V: your network stack and fabric data plane should be designed ring-based from day one.

### 8.5 SmartNICs / DPUs / IPUs

NVIDIA BlueField, Intel IPU, AMD Pensando — NICs that are full computers in their own right. They can run protocols, handle migration, do encryption — all without burdening the host CPU. **A SmartNIC can complete a migration even if the host kernel is paused.**

Your fabric does not need to assume SmartNICs, but the *protocol* should be designed so a SmartNIC could one day handle the data plane.

### 8.6 Hardware page-fault tracking (Intel PML)

**Page Modification Logging (PML)** is an Intel hardware feature: the CPU writes the addresses of dirtied pages into a log buffer automatically. Live migration's "track which pages got modified" no longer needs write-protecting every page — the hardware does it for you, ~5x faster.

For Kernel-V: when you eventually move to a 64-bit kernel with VMX support, PML is the right way to do dirty tracking.

### 8.7 LZ4 / ZSTD compression at memory speed

Modern compressors (LZ4 at 4 GB/s, ZSTD at 1 GB/s with --fast) can compress memory pages *faster than they can be sent over the network*. This means: always compress before sending. Network bandwidth × compression ratio (typically 2-4x for real memory) effectively multiplies your migration speed.

### 8.8 Hardware crypto offload (AES-NI, AES-GCM)

Encrypting all fabric traffic used to be expensive. With AES-NI, it's essentially free (>10 GB/s per core). No reason not to encrypt every byte.

### 8.9 Summary: the math now works

In 2010, migrating a 1 GB process over 1 GbE took **8 seconds**. Unacceptable.

In 2025, on commodity hardware:
- 100 GbE: 1 GB raw = 80 ms
- With 3x compression: 25 ms
- With post-copy (only working set transferred immediately): **5-10 ms downtime**

That's faster than a human can perceive. The "transparency" promise of SSI is finally physically achievable.

---

## 9. Design Principles for Kernel-V DSSI

These are the principles every later document refines. They come from learning from MOSIX's, Kerrighed's, and openSSI's failures.

### 9.1 Eventually-consistent, not coherent

Do not try to give all nodes a perfectly synchronized view of global state. That way lies CAP-theorem pain. Instead: every node has a *best-effort recent* view of every other node, updated by gossip. Migration decisions are made on slightly stale info — and that is fine. A node 200 ms old is still useful.

### 9.2 Home-node, not no-node

Every process has a **home node** — the node where it was spawned. File descriptors, signals, parent-child relationships always involve the home node. When the process is migrated, it becomes a **guest** on the host node, but the home node is its anchor. This is MOSIX's deputizing model and it is dramatically simpler than full distributed state.

### 9.3 Opt-in transparency

Not every process should migrate. A real-time process, a process holding a hardware lock, a process with a GPU buffer — these must pin to one node. The fabric must respect a `MIGRATABLE` flag. Default: **opt-in** until you trust the system; eventually flip to opt-out for normal compute jobs.

### 9.4 Two planes, not one stack

Control plane (gossip, scheduling decisions) and data plane (page transfer, syscall proxy) are *separate subsystems*. Different protocols, different threads, different tuning. Mixing them is a classic mistake.

### 9.5 Observable from day one

Every fabric event must emit a TraceOS record. You cannot debug a distributed system you cannot see. Every page transfer, every migration, every gossip message — traced, timestamped, dumpable.

### 9.6 Fail gracefully, never silently

If a node disappears mid-migration, the user process must either continue on the source or fail loudly. Never enter a zombie state where the process exists nowhere. **Use the source node as the rollback** — do not commit the migration until the destination has the process fully restored and running.

### 9.7 Security is not optional, but layered

There are three security postures:
- **Trusted fabric** (your laptop + your desktop): no encryption needed, simple shared secret
- **Semi-trusted fabric** (you + friends): authentication required, optional encryption
- **Untrusted fabric** (internet-scale): full encryption, capability tokens, code attestation

The design must support all three, but Phase 16-22 only need the first.

### 9.8 Build the primitive, validate, then build the abstraction

Same as your existing Kernel-V philosophy. Build stop-and-copy migration first. Get it working. *Then* build pre-copy on top. *Then* post-copy. Each is a generation of refinement, not a rewrite.

### 9.9 Use the kernel you already have

The fabric layer is a **kernel thread**, not a new privilege ring. Your existing `PROC_TYPE_KERNEL` is exactly what's needed. This keeps debugging simple — you can `printk` from the fabric, you can set breakpoints, you can dump state with your existing tools.

### 9.10 Modern hardware, but degrade gracefully

Design for RDMA, SmartNICs, 100 GbE. But the fabric must work on a `virtio-net` link in QEMU. Always have a software fallback for every hardware optimization.

---

## 10. Glossary of Terms

This glossary will be referenced by every later document. Worth bookmarking.

| Term | Meaning |
|------|---------|
| **DSSI** | Distributed Single System Image. The class of OS designs Kernel-V is aiming for. |
| **Fabric** | The set of all Kernel-V instances cooperating as one system. |
| **Node** | A single Kernel-V instance. Has a unique `node_id`. |
| **Home node** | The node where a process was originally spawned. Owns its identity, file descriptors, parent. |
| **Host node** | The node where a process is currently *executing* (may differ from home node after migration). |
| **Guest process** | A process executing on a host node that is not its home node. |
| **Control plane** | The subsystem managing fabric membership, resource state, migration decisions. Low bandwidth, high frequency. |
| **Data plane** | The subsystem transferring process state (memory, registers) and proxying syscalls. High bandwidth. |
| **Checkpoint** | Serialized snapshot of a process's full state (registers, stack, memory, FDs). |
| **Restore** | Reconstruction of a process from a checkpoint. May be on the same or different node. |
| **Stop-and-copy migration** | Freeze process, copy everything, restart on destination. Simple, has user-visible downtime. |
| **Pre-copy migration** | Copy memory while process still runs, iterate on dirty pages, brief stop-and-handoff. |
| **Post-copy migration** | Stop process, copy minimal state, restart on destination, fault pages in on demand. |
| **Dirty page tracking** | Detecting which pages a running process has modified, for incremental migration. |
| **PML** | Page Modification Logging — Intel hardware feature for fast dirty tracking. |
| **Deputizing** | MOSIX term for "this syscall executes on the home node even though the process is on a host node." |
| **Syscall proxy** | The mechanism by which a guest process's syscalls are forwarded back to the home node. |
| **Gossip protocol** | An eventually-consistent way for nodes to share state by periodically exchanging summaries with random peers. |
| **Phi Accrual failure detector** | A probabilistic failure detector that scales suspicion smoothly instead of binary alive/dead. |
| **RDMA** | Remote Direct Memory Access. Hardware reads/writes another machine's memory bypassing its CPU. |
| **RoCEv2** | RDMA over Converged Ethernet v2 — RDMA on standard routable Ethernet. |
| **CXL** | Compute Express Link — modern interconnect for cache-coherent disaggregated memory. |
| **DSM** | Distributed Shared Memory — abstraction where memory pages can exist on any node with coherence maintained. |
| **Working set** | The pages a process actually accesses in a given time window. Usually much smaller than total memory. |
| **Convergence (pre-copy)** | The state where the rate of new dirty pages is less than the network transfer rate, so the iteration terminates. |
| **Downtime (migration)** | The time during which the process is paused, neither running on source nor destination. |
| **Capability token** | A cryptographic credential authorizing a specific action (e.g., "this process may run on Node C"). |
| **9P** | Plan 9's filesystem protocol — possibly an inspiration for our control plane. |
| **Noise Protocol** | A modern cryptographic handshake framework (used by WireGuard) — candidate for node-to-node auth. |
| **io_uring** | Linux's modern shared-ring-buffer I/O API — design inspiration for our data plane. |
| **Soft-RoCE** | Software implementation of RDMA over Ethernet — lets us prototype RDMA semantics on any NIC. |

---

## 11. Success Criteria & Demo Milestones

Concrete, demonstrable milestones to keep the project honest:

### Milestone M0 (end of Phase 11, ~month 6) — *"It pings"*
> Two Kernel-V instances in QEMU exchange ICMP pings.

### Milestone M1 (end of Phase 13, ~month 8) — *"It explains itself"*
> Run `ledger pid 5` and see per-process CPU, memory, I/O, and a causal trace of what it did. No fabric yet — but the data needed for fabric decisions exists.

### Milestone M2 (end of Phase 15, ~month 10) — *"It snapshots"*
> Run `checkpoint pid 5 > /tmp/snap`, kill the process, run `restore /tmp/snap`, and the process continues from exactly where it left off. Still on one node.

### Milestone M3 (end of Phase 17, ~month 12) — *"They see each other"*
> Start three Kernel-V instances. Run `fabric peers` on any of them. See all three listed with their CPU/memory state. No migration yet.

### Milestone M4 (end of Phase 19, ~month 15) — **THE FIRST REAL DEMO**
> Start a compute-heavy process on Node A. Run `fabric migrate <pid> --to NodeC`. The process resumes on Node C and finishes. Its output (write syscalls) is proxied back to Node A's terminal. **A process born on one machine completed on another.**

### Milestone M5 (end of Phase 22, ~month 19) — *"It migrates live"*
> Same as M4, but the process never appears to stop. Downtime < 50 ms. User watching it does not notice.

### Milestone M6 (end of Phase 23, ~month 22) — *"It chooses on its own"*
> Spawn 10 CPU-heavy processes on Node A while Node B and C are idle. The fabric scheduler automatically redistributes them. User typed nothing about migration.

### Milestone M7 (end of Phase 24, ~month 24) — *"It reads anywhere"*
> A process migrated to Node C opens a file. The file is physically on Node A. Read succeeds, transparently, at network speed. `cat /a/file > /b/file` works across the fabric.

### Milestone M8 (Phase 28, open-ended) — *"It scales to the internet"*
> A friend on the other side of the country runs Kernel-V. You connect. You run a process that uses some of their idle GPU. **Personal cloud, end of cloud monopoly era.**

---

## 12. Risks, Tradeoffs, and What We Will NOT Build

Honest scoping:

### 12.1 What we will NOT build (at least not in the first 2 years)

- **Distributed shared memory with strong coherence.** Kerrighed died on this. Pages live where they live. The process moves to the pages, not pages to the process.
- **Cross-architecture migration (x86 ↔ ARM).** Popcorn Linux territory. Massive complexity. Keep all fabric nodes on the same ISA.
- **Multi-threaded process migration with shared memory.** Single-threaded processes only for the first few generations. Multi-threaded comes later (requires migrating thread groups atomically).
- **GPU passthrough across the fabric.** GPU state migration is essentially unsolved. Local-only for GPU work.
- **Full POSIX compliance for migrated processes.** Some syscalls (e.g., `mmap` with `MAP_SHARED` across processes on different nodes) will simply not work. We document the supported subset.
- **Byzantine fault tolerance.** We assume nodes are honest-but-may-crash. Malicious nodes are a Phase 27+ concern.

### 12.2 Honest tradeoffs

| We chose | We gave up |
|----------|-----------|
| Process-level migration | Container-level migration (easier but less magical) |
| Home-node forwarding | True distributed file system (Kerrighed-class complexity) |
| Eventually-consistent control plane | Globally accurate scheduling decisions |
| 32-bit x86 first | Modern features (PML, CXL, AVX-512) — punted to 64-bit port |
| Custom fabric protocol | Reuse of existing distributed-systems stacks (gRPC, etc.) |

### 12.3 The biggest risks

1. **The 32-bit x86 ceiling.** 32-bit limits us to 4 GB per process and lacks modern dirty-tracking hardware. **Mitigation:** Phase 14+ should consider a 64-bit port in parallel. We design APIs to be 64-bit clean even on 32-bit.
2. **Network stack scope creep.** A full TCP/IP stack is itself a 6-month project. **Mitigation:** Phase 11 builds UDP + a custom reliable transport on top. TCP comes later, optionally.
3. **Checkpoint/restore complexity.** CRIU is 100k+ lines for Linux. Our scope is smaller, but still huge. **Mitigation:** Phase 15 only handles processes with simple state — no threads, no shared memory, no signals queued. Grow scope generationally.
4. **The "no users" problem.** If you build this and nobody uses it, you cannot find real bugs. **Mitigation:** Build a *demo app* that genuinely benefits — distributed compile (`make -j` across nodes) is the killer demo.
5. **Burnout.** Two years is long. Phases must each end with a working, demonstrable result. Never go more than 2 weeks without a green test.

---

## 13. Reading Map

Depending on what you want to do next:

### "I want to keep building Kernel-V Phase 6-11 today."
→ Read **Document 01** (`dssi_01_foundation_seeds.md`). It tells you what small DSSI-aware design decisions to make in your existing phases so they help DSSI later.

### "I want to design the observability layer."
→ Read **Document 02** (`dssi_02_observability.md`).

### "I want to understand the single hardest problem (process migration)."
→ Read **Document 03** (`dssi_03_checkpoint_restore.md`).

### "I want to design the management layer protocol."
→ Read **Documents 04 + 05** (`dssi_04_fabric_control_plane.md`, `dssi_05_first_remote_exec.md`).

### "I want to see the holy grail (live migration)."
→ Read **Document 06** (`dssi_06_live_migration.md`).

### "I want the futuristic stuff (RDMA, CXL, internet-scale)."
→ Read **Document 07** (`dssi_07_advanced.md`).

---

## Closing Note

This is a **two-year voyage**. You are not building a product. You are building a piece of systems research — the kind of project that, even if it never sees widespread adoption, will teach you more about how computers actually work than any tutorial, any course, or any job.

MOSIX existed. Kerrighed existed. They died not because the idea was wrong, but because the world wasn't ready and the hardware was too slow. The world *is* ready now. Personal computing is fragmented into a dozen devices per person, all sitting at 5% utilization 95% of the time. Cloud computing extracts a tax for solving a problem that should be solvable locally.

Kernel-V DSSI is a small bet that **the right software, on top of today's hardware, can give every person a private cloud made of the machines they already own.**

That is the north star. Every later document refines a piece of how we get there.

---

**Next document to write:** `dssi_01_foundation_seeds.md` — the small DSSI-aware decisions to make *now* in your Phase 6-11 work so the fabric layer slots in smoothly later.
