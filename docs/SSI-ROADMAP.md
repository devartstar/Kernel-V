# Kernel-V — Distributed Single System Image (SSI) Master Roadmap

> **North Star:** an elegant distributed operating system where a community of
> nodes — a datacenter's machines or strangers on the internet — pool their
> CPU, memory, storage, and devices over the network and present them as **one
> logical computer**, so no user is ever hard-limited by a single box.
>
> **Organizing principle:** *everything is a file, and any file can live on any
> node.* A uniform, location-transparent namespace is the spine; CPU, memory,
> devices, and storage are all expressed through it.

---

## 0. How to read and use this document

- The project is organized as **Parts → Phases → Subphases → Tasks**.
- Near-term work (Parts I–IV) is broken down in fine detail. Far-term work
  (Parts V–IX) is deliberately coarser — we sharpen it as we approach it.
- Track progress by checking boxes: `- [ ]` → `- [x]`. Each Phase has an
  **Exit criterion** (a demo or test that proves it's done).
- **Milestones (Mn)** are the motivational spine — each is a runnable demo that
  proves a slice of the thesis end-to-end.
- We keep the *teaching loop*: designs discussed first, code written by the
  author, reviewed and compiled green before moving on.

**Status legend:** ✅ done · 🚧 in progress · ⬜ not started · 🧊 deferred

---

## 1. Vision, thesis, and honest reality checks

### 1.1 The thesis
Kernel-level location transparency lets **unmodified programs** transparently
consume remote resources. Instead of the app-developer stitching together a
distributed system (Kubernetes, Spark, Ray), the *kernel* makes "the cluster"
look like one machine. Resources become fluid.

### 1.2 Why this could be the next big thing
- **Federation is underexplored.** Classic SSI (MOSIX, OpenSSI, Kerrighed) was
  single-owner clusters. A *mutually-distrusting community* pooling resources
  with capabilities + accounting is genuinely fresh territory.
- **Hardware finally cooperates.** Fast NICs, RDMA, and CXL memory pooling make
  remote resources far cheaper to reach than in the 2000s when kernel SSI lost.
- **Transparency is a real moat.** "Run any binary, use the whole community's
  resources, change nothing" is a value proposition orchestration can't match.

### 1.3 Reality checks (read these more than once)
1. **This is a multi-year effort; scope success as a **compelling prototype**,
   not a datacenter product.** Target: prove the full thesis on 2–3 QEMU nodes
   with an elegant, defensible design. "Replace Kubernetes" is not a v1 goal.
2. **History warns us.** Kernel SSI historically lost to userspace/containers
   because of *failure isolation* and *complexity*. Our differentiators must
   stay sharp: transparency + federation + modern-hardware fast paths. If a
   feature doesn't serve one of those, question it.
3. **Physics is undefeated.** Remote memory is ~1000× slower than local. Share
   **coarse-grained** first (migrate a process, remote-open a device/file);
   treat transparent fine-grained distributed shared memory as a *late,
   optional* research goal, not a foundation.
4. **Partial failure is the normal case, not an edge case.** "A piece of me just
   vanished" must be designed in from Layer 1, not bolted on.
5. **Don't build a Unix clone.** Every primitive must be *pulled into existence
   by a concrete SSI need*, not added because Linux has it.
6. **Security is not a phase you reach later — it gates the very first remote
   byte.** A 2-node demo that skips auth teaches the wrong architecture.

### 1.4 Design tenets (the constitution)
- **Everything is a file / named object.** One access model, remote-able.
- **Location transparency.** Callers never branch on "local vs remote."
- **Capability security.** Access = holding an unforgeable handle, not identity
  checks scattered in code.
- **Serializable state.** Any migratable thing (process, session) must avoid
  unmovable self-referential blobs. (We already live this: "never memcpy a live
  session.")
- **Coarse-grained sharing before fine-grained.**
- **Design for partial failure and timeouts everywhere.**
- **Goal-driven primitives.** Justify each core primitive by the spine's need.

---

## 2. Milestone spine (the demos that prove the thesis)

| # | Milestone demo | Proves | Depends on |
|---|----------------|--------|------------|
| M0 | ✅ Multi-session TTY, isolated | location-transparent device *pattern* | Parts done so far |
| M1 | ⬜ `kmalloc`/`kfree` stress test passes | dynamic memory | A0 |
| M2 | ⬜ Two QEMU nodes exchange a reliable, acked message | transport | A0–A3, N0–N3 |
| M3 | ⬜ Node A invokes an RPC on Node B, gets a reply | kernel RPC | N4 |
| M4 | ⬜ Node A mounts Node B's namespace, reads a file | distributed VFS | Part III, IV |
| M5 | ⬜ **Node A `open("/net/B/dev/tty1")` and drives B's terminal** | **the thesis, end to end** | G-series |
| M6 | ⬜ Node A spawns a process that runs on Node B | remote exec | Part VI |
| M7 | ⬜ A running process migrates A→B and keeps going | migration | R3 |
| M8 | ⬜ A borrows B's CPU under a quota; accounting records it | federation/fairness | Part V |
| M9 | ⬜ 3-node cluster survives one node dying mid-work | resilience | Part VII |
| M10 | ⬜ An *unmodified* program transparently uses pooled resources | transparency payoff | all above |

---

## 3. Part 0 — Foundations already built (context)

- [x] ✅ Boot, GDT/IDT, paging, physical + virtual MM, DMA
- [x] ✅ Processes, scheduler, context switch, usermode, syscalls (minimal set)
- [x] ✅ Channel-based wait + signals (proc_signal_channel, sigpending)
- [x] ✅ VFS + ramfs + devfs + fd table
- [x] ✅ PCI subsystem + VirtIO Block (virtqueue/descriptor machinery) — *reused
      directly by virtio-net later*
- [x] ✅ TTY: pipeline line discipline, flyweight stages, multi-session,
      controlling-tty node (Phases 1–6, checkpoints 001–007)

> **Leverage note:** PCI + virtio-block means the NIC (N0) is *not* greenfield —
> it reuses existing virtqueue and PCI-config code. Biggest single accelerator
> in the whole plan.

---

## PART I — Core kernel primitives the spine demands

*These are the "don't skip" gaps. Each is justified by the network layer that
immediately follows.*

### Phase A0 — Dynamic kernel heap (`kmalloc`/`kfree`)  ⬜
*Why now:* packet buffers, connection state, remote-object tables, RPC messages
are all variable-size and dynamic. Pools alone can't express them.

- **A0.1 Design** ⬜
  - [ ] Choose allocator strategy: free-list + coalescing vs. slab-on-pools vs.
        buddy. (Recommendation: start with a **segregated free-list / K&R-style
        heap** over page-allocated arenas; add slab caches later for hot types.)
  - [ ] Decide arena source: grab pages from PMM; grow on demand.
  - [ ] Alignment, min block size, header layout, overflow/red-zone strategy.
- **A0.2 Implement core** ⬜
  - [ ] `kmalloc(size)`, `kfree(ptr)`, `krealloc`, `kcalloc`.
  - [ ] Arena growth (request pages from PMM, map into kernel heap region).
  - [ ] Free-block coalescing to fight fragmentation.
- **A0.3 Hardening** ⬜
  - [ ] Guard bytes / poison-on-free; double-free detection (debug build).
  - [ ] Stats: bytes in use, peak, allocation count (feeds observability later).
- **A0.4 Tests** ⬜
  - [ ] Unit: alloc/free patterns, alignment, coalescing, fragmentation stress.
  - [ ] **M1**: randomized alloc/free stress test survives N iterations.
- **Exit criterion:** M1 green; a subsystem can allocate arbitrary-size objects.

### Phase A1 — Blocking primitives (wait-queues, semaphores, mutexes)  ⬜
*Why now:* the moment I/O blocks (`recv`, RPC call), we need a clean "sleep
until event" primitive. We already have the seed: `proc_wait_prepare_on(channel)`.

- **A1.1 Generalize wait-queues** ⬜
  - [ ] `wait_queue_t` with `wq_wait(wq)` / `wq_wake_one` / `wq_wake_all`.
  - [ ] Refactor existing channel-wait + tty read to sit on this abstraction.
- **A1.2 Semaphore / mutex / completion** ⬜
  - [ ] Counting semaphore, sleeping mutex, `completion` (one-shot event).
  - [ ] Timed variants (`wq_wait_timeout`) — needs A3.
- **A1.3 Correctness** ⬜
  - [ ] Lost-wakeup safety, IRQ-context rules documented, spurious-wakeup loops.
- **A1.4 Tests** ⬜
  - [ ] Producer/consumer, mutex mutual-exclusion, timeout wakeups.
- **Exit criterion:** two kernel threads coordinate via semaphore + wait-queue.

### Phase A2 — Deferred work (bottom-halves + kernel threads)  ⬜
*Why now:* a NIC raises IRQs fast; the handler must stay tiny and defer real
work. You learned this with serial (IRQ → drain). Generalize it.

- **A2.1 Kernel threads** ⬜
  - [ ] First-class `kthread_create(fn, arg, name)` (you have `proc_create` with
        a kernel entry — formalize + name + lifecycle).
- **A2.2 Deferred execution** ⬜
  - [ ] A softirq/tasklet-style bottom half **or** a workqueue (kthread + job
        queue). (Recommendation: **workqueue** — simpler mental model, sleeps OK.)
  - [ ] Enqueue-from-IRQ, run-in-thread; backpressure policy.
- **A2.3 Tests** ⬜
  - [ ] IRQ handler enqueues N jobs; worker drains them in order.
- **Exit criterion:** an interrupt can safely schedule blocking follow-up work.

### Phase A3 — Timed events & timeouts  ⬜
*Why now:* retransmit, failure detection, and leases all need timeouts. You have
a tick + `proc_wait_sleep(ticks)`; upgrade to programmable timers.

- **A3.1 Timer infrastructure** ⬜
  - [ ] Timer objects with callbacks; a **timer wheel** or sorted list.
  - [ ] `timer_add(delay, fn, arg)`, `timer_cancel`, monotonic clock.
- **A3.2 Integrate with waits** ⬜
  - [ ] `wq_wait_timeout`, `sema_wait_timeout` built on A1 + A3.
- **A3.3 Tests** ⬜
  - [ ] Timer fires within tolerance; cancel works; timed-wait wakes on timeout.
- **Exit criterion:** a blocking call can time out deterministically.

> **Part I exit:** the kernel can dynamically allocate, block/wake cleanly,
> defer IRQ work to threads, and time things out. *Everything distributed needs
> exactly these four.*

---

## PART II — Networking (the transport spine)

### Phase N0 — NIC driver: virtio-net  ⬜
*Reuses PCI + virtqueue code from virtio-block.*

- **N0.1 Device bring-up** ⬜
  - [ ] PCI discover virtio-net; feature negotiation; MAC read.
  - [ ] Set up RX/TX virtqueues (reuse virtio-block descriptor logic).
- **N0.2 RX path** ⬜
  - [ ] Post RX buffers; IRQ on packet; **bottom-half** (A2) drains ring.
- **N0.3 TX path** ⬜
  - [ ] Enqueue frame on TX vq; completion handling; buffer recycling.
- **N0.4 Tests / bring-up** ⬜
  - [ ] Loopback / observe frames in QEMU; RX+TX counters.
- **Exit criterion:** node can send and receive raw Ethernet frames in QEMU.

### Phase N1 — Link & framing  ⬜
- [ ] Ethernet frame parse/build; our own EtherType for the cluster protocol.
- [ ] MTU handling; frame validation.
- **Decision point:** *do we ride raw Ethernet L2 with node-IDs, or implement a
  minimal IP/UDP?* (Recommendation: **raw L2 + cluster EtherType** for the demo
  — skips IP complexity; revisit for real deployment in Part VIII.)
- **Exit criterion:** two QEMU nodes on a shared virtual switch exchange framed
  cluster packets addressed by node.

### Phase N2 — Node addressing & (optional) minimal L3  ⬜
- [ ] Node identifiers, address resolution (node-ID → MAC).
- [ ] Optional: tiny UDP/IP if we choose routable addressing.
- **Exit criterion:** a packet can be addressed to "node B" symbolically.

### Phase N3 — Reliable message transport  ⬜
*A mini reliable datagram/stream: the network is lossy; we hide it.*

- **N3.1 Segmentation & reassembly** ⬜ (messages larger than a frame)
- **N3.2 Reliability** ⬜ sequence numbers, ACKs, retransmit (uses A3 timers)
- **N3.3 Flow / congestion control (basic)** ⬜ windowing, backpressure
- **N3.4 Connection state** ⬜ per-peer state in `kmalloc`'d structures (A0)
- **N3.5 Tests** ⬜ inject loss/reorder; verify in-order, exactly-once delivery
- **Exit criterion → M2:** two nodes exchange a large message reliably under
  simulated loss.

### Phase N4 — Kernel RPC layer  ⬜
*The programming model everything above uses.*

- **N4.1 Message format** ⬜ compact, versioned, endian-defined wire encoding
- **N4.2 Marshaling** ⬜ encode/decode of typed args & results
- **N4.3 Request/reply** ⬜ correlation IDs, blocking call (A1), async call,
  server dispatch table
- **N4.4 Robustness** ⬜ timeouts, retries (idempotency keys), error propagation
- **N4.5 Tests** ⬜ round-trip typed RPC; timeout + retry; concurrent calls
- **Exit criterion → M3:** node A calls `rpc(B, method, args)` and gets a reply.

---

## PART III — Cluster membership, identity & naming

### Phase C0 — Node identity  ⬜
- [ ] Per-node cryptographic keypair; stable node ID derived from public key.
- [ ] Identity persisted / provisioned; anti-spoofing basis for Part V.

### Phase C1 — Membership & discovery  ⬜
- [ ] Join protocol; peer table; **heartbeats + failure detection** (A3 timers).
- [ ] Bootstrap: static peer list first; discovery/gossip later.

### Phase C2 — Global naming  ⬜
- [ ] Global IDs: `(node, local-id)` for processes, resources, capabilities.
- [ ] Name resolution service over RPC.

### Phase C3 — Cluster view / consensus-lite  ⬜
- [ ] Membership epochs/versioning; agreement on "who is in the cluster."
- [ ] (Full consensus like Raft deferred; start with epoch + coordinator.)
- **Exit criterion:** nodes maintain a consistent, versioned view of live peers
  and detect a dead peer within a bounded time.

---

## PART IV — Global namespace (9P-style distributed VFS) — **the spine**

### Phase G0 — Namespace plumbing  ⬜
- [ ] VFS **mount table**; per-process namespace (if not already present).
- [ ] Confirm the "session = node->private_data, pure resolution" pattern
      generalizes to remote-backed nodes (TTY already proves it locally).

### Phase G1 — Distributed file protocol (9P-like) over RPC  ⬜
- [ ] Protocol ops: `attach, walk, open, read, write, clunk, stat`.
- [ ] Map onto existing `vfs_node_ops_t` (read/write already match!).

### Phase G2 — Namespace server (export)  ⬜
- [ ] A node serves part of its VFS to peers; capability-checked (Part V hook).

### Phase G3 — Namespace client (mount remote)  ⬜
- [ ] Mount peer's export at `/net/<node>`; remote nodes look local.
- [ ] Remote `vfs_node_t` whose ops forward to G1 RPCs (the "network port").
- **Exit criterion → M4:** `read("/net/B/somefile")` returns B's bytes.

### Phase G4 — First distributed device demo  ⬜
- [ ] `/dev/tty1` on B, exported; A mounts it; A drives B's terminal.
- **Exit criterion → M5:** **the thesis, proven end to end on the smallest
  possible surface.** *(This is the make-or-break motivational moment.)*

---

## PART V — Security, trust, capabilities & accounting (federation)

> For a *community* of mutually-distrusting nodes, this is foundational, and its
> hooks are added incrementally starting at G2 — not tacked on at the end.

### Phase S0 — Capability model  ⬜
- [ ] Unforgeable capability handles (cryptographic tokens) to resources.
- [ ] Access = present a valid capability, not an identity check in code.

### Phase S1 — Authentication  ⬜
- [ ] Mutual auth between nodes (keys from C0); session keys; encrypted channel.

### Phase S2 — Authorization policy  ⬜
- [ ] Policy: who may access which exports/resources; delegation of caps.

### Phase S3 — Quotas & accounting (the fairness economy)  ⬜
- [ ] Per-peer resource quotas (CPU-time, memory, storage, bandwidth).
- [ ] **Credit/accounting ledger**: you may borrow ≈ what you lend. Anti-free-ride.
- **Exit criterion → M8:** A borrows B's CPU under a quota; the ledger records it
  and enforces the cap.

### Phase S4 — Sandboxing foreign work  ⬜
- [ ] Isolate remote-originated processes (resource + fault + security domains).
- [ ] Kill-switch / eviction of misbehaving foreign work.

---

## PART VI — Distributed resources (Layer 5 payoff)

### Phase R0 — Remote device access (generalize the TTY demo)  ⬜
- [ ] Any devfs node exportable/mountable; the network-backed `tty_port`.

### Phase R1 — Distributed storage  ⬜
- [ ] Global files via the namespace + a backing store; caching + coherence.

### Phase R2 — Remote process spawn / exec  ⬜
- [ ] Ship a program to B, run it there, wire its stdio back through the
      namespace (reuses controlling-tty-as-node!).
- **Exit criterion → M6.**

### Phase R3 — Process migration  ⬜
- [ ] Checkpoint process state + address space (serializable-state tenet pays
      off); transfer; resume on B; fix up namespace/handles.
- **Exit criterion → M7.** *Hardest single feature; expect multiple subphases
  discovered here.*

### Phase R4 — Remote / distributed memory  ⬜
- [ ] Remote paging / swap-to-remote first (coarse). Transparent DSM = research
      stretch goal, explicitly optional.

### Phase R5 — Distributed scheduler  ⬜
- [ ] Load-aware placement; migration policy; where-should-this-run decisions.

---

## PART VII — Resilience & operations

- **O0 Failure handling** ⬜ node loss, lease recovery, orphan/handle cleanup.
- **O1 Consistency & replication** ⬜ for critical cluster state.
- **O2 Observability** ⬜ distributed tracing (you already have `trace_id`!),
  metrics, per-node dashboards.
- **O3 Live cluster management** ⬜ join/leave, rebalancing, draining a node.
- **Exit criterion → M9:** 3-node cluster survives a node dying mid-work.

---

## PART VIII — Hardening & real-world

- **H0 Real NIC** ⬜ e1000 / real hardware beyond QEMU virtio.
- **H1 Performance** ⬜ zero-copy, batching, RDMA / fast paths, CXL memory.
- **H2 Multi-arch / deployment** ⬜ real machines, provisioning.
- **H3 Security audit & threat model** ⬜ formalize adversary, red-team the caps.

---

## PART IX — Community & ecosystem (product)

- **P0** ⬜ Node onboarding UX; one-command join.
- **P1** ⬜ The credit/market layer for public resource sharing.
- **P2** ⬜ Governance, reputation, abuse handling.
- **P3** ⬜ Datacenter mode vs. public-community mode configuration.
- **Exit criterion → M10:** an unmodified program transparently uses the pool.

---

## 4. Sequencing summary (critical path)

```
Part I (A0→A1→A2→A3)  ──▶ Part II (N0→N1→N2→N3→N4)  ──▶ M2,M3
        │                                   │
        └───────────────┬───────────────────┘
                        ▼
     Part III (C0→C1→C2→C3) ──▶ Part IV (G0→G1→G2→G3→G4) ──▶ M4,M5  ◀── thesis proven
                        │                     ▲
                        ▼                     │ (caps hook in at G2)
     Part V (S0→S1→S2→S3→S4) ─────────────────┘ ──▶ M8
                        ▼
     Part VI (R0→R1→R2→R3→R4→R5) ──▶ M6,M7
                        ▼
     Part VII ──▶ M9    Part VIII    Part IX ──▶ M10
```

**Immediate next action:** Phase **A0 — kernel heap**, starting with A0.1 design
(free-list vs. slab vs. buddy).

---

## 5. Change log
- 2026-09-07 — Initial master roadmap authored (post TTY Phase 6-multi).
