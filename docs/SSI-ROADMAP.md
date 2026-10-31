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
- **Fast and reliable, always — non-negotiable.** Every subsystem is designed
  for the hot path first (latency + throughput) and for correctness under
  failure second-to-none. Concretely this means, at every layer:
  - *Predictable performance:* O(1)/O(log n) hot paths, no hidden allocation in
    fast paths, bounded lock hold times, cache-friendly layout, zero-copy where
    possible, batching over per-item overhead.
  - *Reliability by construction:* explicit error paths, no silent drops,
    timeouts on every wait, idempotent retries, invariants asserted in debug
    builds, and every feature shipped with tests that prove it.
  - *Measure, don't guess:* each core subsystem exposes counters/latency stats
    so regressions are visible (ties into Observability, O2).
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
- [x] ✅ PCI subsystem: enumeration, BAR/capability parsing, driver registry +
      bind; DMA page allocator; MMIO helpers. *(No virtio yet — built in N1.)*
- [x] ✅ TTY: pipeline line discipline, flyweight stages, multi-session,
      controlling-tty node (Phases 1–6, checkpoints 001–007)

> **Leverage note:** the PCI driver model, capability walker, and
> `dma_alloc_pages` mean virtio-net (N1) plugs into existing infrastructure;
> only the virtio transport + virtqueue library are new.

---

## PART I — Core kernel primitives the spine demands

*These are the "don't skip" gaps. Each is justified by the network layer that
immediately follows.*

### Phase A0 — Dynamic kernel heap: **KAlloc**  ✅
*Why now:* packet buffers, connection state, remote-object tables, RPC messages
are all variable-size and dynamic. Pools alone can't express them.

**Locked design — KAlloc: a tiered allocator under one unified page descriptor.**
Route each size range to the family that is optimal for it, but resolve every
pointer the same O(1) way (`addr & ~0xFFF` → page/segment descriptor). Design
tenets: **deterministic worst-case O(1)**, **bounded internal fragmentation**,
and an **SSI-forward arena seam** (arenas are the future unit of NUMA/node
affinity and RDMA registration — a local pointer may one day bridge to a remote
node's RAM, mirroring "a terminal is a named node whose port can be remote").

| Tier | Range | Family | Notes |
|------|-------|--------|-------|
| **T0** | substrate | buddy over the 16 MB heap window | feeds upper tiers; O(log n) coalescing; *stubbed as a simple page-run bump first* |
| **T1** | ≤ 2 KB | **slab, quarter-spaced size classes** | page-header slab (zero per-object overhead), per-CPU magazine-ready, O(1). **The 90% path — build first.** |
| **T2** | 2–64 KB | TLSF (two-level segregated fit) | O(1) alloc+free, tight fit for odd sizes; *deferred* |
| **T3** | > 64 KB | page-run from T0 | header cost negligible; *simple first* |

**Distinctive commitments:** (1) worst-case O(1) at *every* tier; (2) one
unified page/segment descriptor → no per-object header, no free-list search on
free; (3) arena = node-affinity/RDMA seam (designed-for, deferred).
**Build discipline (reality check):** design the descriptor + interface for all
four tiers now, but implement only **T1 slab + a trivial large/page-run path**
first. Add magazines, buddy T0, and TLSF T2 later only when profiling demands.

**Ground truth (corrected — this was a real bug, see A0.5):** KAlloc pages come
from the reserved kernel heap **virtual** window (`KERNEL_HEAP_START..END`) and
are explicitly backed via `paging_map_page()`. Only the first 4 MB of physical
RAM is identity-mapped, so a raw `pmm_alloc_frame()` address is **not** directly
dereferenceable once allocations climb past low memory — the initial "frames are
directly dereferenceable" assumption caused a page fault post-merge. Pages stay
4 KB aligned so `ptr & ~0xFFF` still lands on the descriptor; freed pages are
cached mapped for O(1) reuse.

- **A0.1 Design** ✅ *(locked: KAlloc, above)*
  - [x] Allocator strategy: tiered slab + TLSF + page-run under unified descriptor.
  - [x] Size classes: **quarter-spaced** (bounded ~25% internal waste) not pure powers of two.
  - [x] Pointer→metadata: page-header slab, O(1) `addr & ~0xFFF`, zero per-object overhead.
  - [x] Arena source: PMM frames (identity-mapped, like `pool_alloc.c`); heap window reserved for T0/large + future node-affine arenas.
- **A0.2 Implement core (T1 + trivial large path)** ✅
  - [x] A0.2a Descriptor + size-class table: `kmem_page_desc_t`, `size→class` O(1) lookup, class table.
  - [x] A0.2b Slab engine (**per-page free list + partial-list + O(1) reclaim**): each page owns its free list + `in_use`; cache keeps a doubly-linked partial-page list; `slab_alloc`/`slab_free` O(1); emptied pages return to PMM.
  - [x] A0.2c Public API: `kmalloc(size)`, `kfree(ptr)`, `kcalloc`, `krealloc`; large path via page-run.
  - [x] A0.2d Init + wire into build (auto-discovered `.c`).
- **A0.3 Hardening** ✅
  - [x] Magic/tier tag in descriptor; double-free detection; poison-on-free (debug).
  - [x] Per-class/per-tier stats: bytes in use, peak, alloc/free counts (observability seam).
- **A0.4 Tests** ✅
  - [x] Unit: size→class rounding, alignment, alloc/free reuse, cross-class isolation.
  - [x] **M1**: randomized alloc/free stress test survives N iterations (verified in QEMU: 19/19).
- **Exit criterion:** ✅ M1 green; a subsystem can allocate arbitrary-size objects.

- **A0.5 Buddy T0 substrate**  ⬜ *(deferred — designed-for, not yet built)*
  *Why it exists:* today T0 is a **bump pointer** (`g_heap_brk`) over the heap
  window plus a mapped-page free cache. That is O(1) and correct, but it has two
  limits the SSI spine will eventually hit:
  1. **No multi-page contiguous allocations.** The large tier is capped at one
     frame (~4064 B usable). DMA ring buffers, big RPC payloads, and page-table
     bulk needs will want contiguous multi-page runs.
  2. **No coalescing of the window itself.** Freed single pages are cached, but
     the window only ever grows toward `KERNEL_HEAP_END`; there is no merging of
     adjacent free regions into larger runs.
  *What T0 becomes:* a **buddy allocator** managing the 16 MB heap window as
  power-of-two page runs (orders 0..N). `alloc(order)` splits a larger block if
  needed; `free` finds its buddy via `addr ^ block_size` and coalesces upward.
  Slab (T1) and page-run (T3) then both draw from T0 instead of the bump.
  *Hard lesson baked in (from the post-merge crash):* the buddy's own metadata
  (free-list heads / order bitmaps) **must be self-placed from a source of
  truth** — sized from the window extent and carved from the window itself or
  anchored to `kernel_end` — **never a hardcoded address**. The PMM frame bitmap
  was pinned at `0x90000`, silently fell inside the grown kernel `.bss`, and
  aliased `current_proc` → page fault. T0 must not repeat that class of bug.
  - [x] A0.5a Buddy structures: per-order free lists + a split/merge bitmap,
        all derived from the window size (no magic addresses).
  - [x] A0.5b `buddy_alloc(order)` / `buddy_free(ptr)` with O(log n) coalescing;
        replace the `g_heap_brk` bump as the T0 page source.
  - [x] A0.5c Multi-page contiguous large tier (T3) on top of T0; lift the
        one-frame cap in `kmem_large_alloc`.
  - [-] A0.5d Tests: contiguous runs, buddy coalescing, fragmentation stress,
        metadata-placement assertion (bitmap never overlaps `.bss`).
  - **Trigger to build:** first consumer needing >4064 B contiguous (DMA rings
    for the NIC in N0, or large RPC buffers), or observed window fragmentation.
  - **Exit criterion:** a multi-page contiguous alloc/free stress test passes;
    T1/T3 both source pages from the buddy; no hardcoded metadata addresses.

*Post-merge fixes already landed (A0 addenda):* (1) KAlloc now sources pages
from the demand-paged heap window via `paging_map_page` (not raw physical
frames); (2) the PMM frame bitmap is anchored to `PAGE_ALIGN_UP(kernel_end)`
instead of hardcoded `0x90000`. Both verified in QEMU.

### Phase A1 — Blocking primitives (wait-queues, semaphores, mutexes)  ✅
*Why now:* the moment I/O blocks (`recv`, RPC call), we need a clean "sleep
until event" primitive. Built on the ported channel-wait seed.

- **A1.0 Port channel-wait** ✅
  - [x] `proc_wait_info_t {reason, tick_count, channel}` in the PCB; the wait
        object's own address is the wait channel (no new list).
  - [x] `proc_wait_prepare_on`, `proc_wakeup_all_on`, `proc_wakeup_one_reason`.
  - [x] Timer handler guards on `PROC_WAIT_SLEEP` so object waiters aren't woken
        every tick (fixed a latent bug during the port).
- **A1.1 Semaphore** ✅
  - [x] `sem_init/sem_wait/sem_post`; check-and-park under `irq_save` defeats the
        lost-wakeup race; `while`-loop re-check on wake.
- **A1.2 Mutex** ✅
  - [x] Owner-tracked binary semaphore; `panik` on non-owner unlock and recursive
        self-lock.
- **A1.3 Tests** ✅
  - [x] Producer/consumer + mutual-exclusion (counter == 10 under an in-CS
        `yield`), **verified in QEMU**: consumer blocks on `sem_wait`, producer
        `sem_post` wakes it across processes.
- *Deferred:* generic `wait_queue_t` abstraction + timed variants
  (`wq_wait_timeout`, needs A3) — fold in when a second consumer appears.
- **Exit criterion:** ✅ two kernel threads coordinate via semaphore + mutex.

### Phase A2 — Deferred work (bottom-halves + kernel threads)  ✅
*Why now:* a NIC raises IRQs fast; the handler must stay tiny and defer real
work. You learned this with serial (IRQ → drain). Generalize it.

- **A2.1 Kernel threads** → moved to **N0.2** (`kthread_create` + stop)
- **A2.2 Deferred execution** ✅
  - [x] Workqueue (kthread + job ring); sleeps OK in work items.
  - [x] Enqueue-from-IRQ, run-in-thread; backpressure = drop + count.
- **A2.3 Tests** ✅
  - [x] IRQ handler enqueues N jobs; worker drains them in order.
- **Exit criterion:** ✅ an interrupt can safely schedule blocking follow-up work.

### Phase A3 — Timed events & timeouts  ✅
*Why now:* retransmit, failure detection, and leases all need timeouts.

- **A3.1 Timer infrastructure** ✅
  - [x] Caller-owned `ktimer_t`; 256-bucket hashed timer wheel; absolute deadlines.
  - [x] `ktimer_add` (re-arm semantics), `ktimer_cancel`, wrap-safe `time_after`, monotonic `ktime_ticks`.
- **A3.2 Integrate with waits** ✅
  - [x] `proc_schedule_timeout` / `proc_park_timeout` (stack timer, mandatory cancel); O(n) sleep scan removed; idempotent `proc_wakeup`.
  - [x] `sem_wait_timeout`, `mutex_lock_timeout` (absolute deadline across spurious wakes).
- **A3.3 Tests** ✅
  - [x] Fires within tolerance; cancel is final; re-arm; timed-wait timeout; try-lock; event-beats-timeout.
- **Exit criterion:** ✅ a blocking call can time out deterministically.

> **Part I exit:** the kernel can dynamically allocate, block/wake cleanly,
> defer IRQ work to threads, and time things out. *Everything distributed needs
> exactly these four.*

---

## PART II — The Fabric: networking from wire to kernel RPC  🚧
*Detailed Phase 2 plan. Decisions locked 2026-10-31.*

**Ground truth (2026-10-30):** PCI bus/bind/BAR/capability parsing,
`dma_alloc_pages` (kva + phys), `mmio_*`, `irq_register_handle`, workqueue,
ktimers and timed sem/mutex exist. **No virtio code exists yet** — the virtio
transport and virtqueue library are built here (N1), not reused.

**Locked decisions**
| # | Decision | Choice |
|---|----------|--------|
| D1 | Virtio flavor | **Modern virtio 1.x** (PCI vendor caps + MMIO); no legacy port-I/O path |
| D2 | Carrier | **Raw Ethernet L2** + our own protocol: TCP's reliability, ordering, flow and congestion control combined with UDP's message boundaries and low overhead, and no head-of-line blocking across lanes |
| D3 | QEMU backend | **`-netdev dgram`** (UDP-backed virtual wire, no root) |
| D4 | Byte order / extensibility | **LE payload, BE Ethernet fields.** Carrier-agnostic header with version, `next_proto`, and TLV options, so the same frame can later ride UDP/IP, RDMA, or a gateway to other protocols |
| D5 | Frame authentication | **SipHash-2-4** cluster-key MAC now; AEAD with per-peer keys in S1 |
| D6 | Transport model | **Messages + planes/lanes.** Control plane (ACK, heartbeat, membership, RPC control; never flow-blocked, highest priority) and data plane (interactive and bulk lanes) |

**Signature ideas:** carrier-agnostic header · seeded fault-injecting
`simwire` (any transport bug replayable by seed) · boot incarnations (peer
reboot detected in one RTT; in-flight calls fail fast) · planes/lanes (TTY
never queues behind migration) · deadline *budgets* (no clock sync) ·
at-most-once RPC reply cache · cross-node trace stitching · frames
authenticated from the first remote byte · load-vector beacons · `/net/peers`
as a file.

### Phase N0 — Bench & building blocks  🚧
- [x] N0.1 Roadmap reconcile (this section; A2/A3 status; virtio correction)
- [ ] N0.2 `kthread_create(fn, arg, name)` + cooperative stop
- [ ] N0.3 Two-node QEMU lab: `make node-a`/`node-b`, shared `dgram` L2, fixed MACs, per-node pcap
- [ ] N0.4 `netbuf`: headroom/tailroom packet buffer, refcount, fixed pool (no alloc in IRQ)
- [ ] N0.5 Wire helpers: BE/LE load/store + bounds-checked cursor reader/writer
- **Exit:** two QEMUs boot side by side with NICs visible on PCI; netbuf + cursor unit tests green.

### Phase N1 — Virtio core + virtio-net  ⬜
- [ ] N1.1 virtio-pci modern transport: vendor caps (common/notify/ISR/device), BAR mapping, status handshake, feature negotiation
- [ ] N1.2 Split-virtqueue library (desc/avail/used, free list, barriers) + fake-device unit test
- [ ] N1.3 virtio-net probe via PCI driver registry; MAC, link status
- [ ] N1.4 `netdev` abstraction (ops, MAC, MTU, stats, RX hook) + loopback netdev
- [ ] N1.5 RX path: pre-posted buffers, IRQ → workqueue, NAPI-lite interrupt suppression, repost
- [ ] N1.6 TX path: `virtio_net_hdr` in headroom, notify, completion reclaim, ring-full backpressure
- [ ] N1.7 Tests: raw broadcast A→B visible in pcap; RX/TX/drop counters
- **Exit:** two nodes exchange raw Ethernet frames.

### Phase N2 — Fabric link layer (KVF)  ⬜
- [ ] N2.1 Ethernet parse/build + EtherType demux table; KVF on `0x88B5`
- [ ] N2.2 KVF header: version, type, flags, next_proto, src/dst node-id, incarnation, plane/lane, len, trace, TLV options
- [ ] N2.3 Node identity: 64-bit node-id (config now, pubkey hash in C0) + boot incarnation nonce
- [ ] N2.4 HELLO beacons + neighbor table (node-id → MAC, incarnation, last-seen, load vector)
- [ ] N2.5 Frame authentication (SipHash-2-4); reject + count
- [ ] N2.6 Wireshark Lua dissector for KVF
- **Exit:** `kvf_send(node_B, …)` by symbolic id; forged or corrupt frames dropped and counted.

### Phase N3 — Reliable message transport (KVT)  ⬜
- [ ] N3.0 `simwire`: seeded loss/dup/reorder/delay/corrupt carrier
- [ ] N3.1 Peer connection state machine + handshake; incarnation change ⇒ reset + fail fast
- [ ] N3.2 Sequence numbers, cumulative ACK + SACK bitmap, dup suppression, replay window
- [ ] N3.3 Retransmission: Jacobson/Karels RTO, Karn's rule, backoff, fast retransmit
- [ ] N3.4 Message fragmentation + bounded-memory reassembly with timeout
- [ ] N3.5 Planes/lanes: per-lane ordering; control > interactive > bulk scheduling
- [ ] N3.6 Flow + congestion control: receiver credits, AIMD cwnd, timed sender backpressure
- [ ] N3.7 Peer health: keepalive, idle probes, UP/SUSPECT/DOWN events (feeds C1)
- [ ] N3.8 Seed-sweep tests: exactly-once, per-lane order, bounded memory under loss
- **Exit → M2:** 256 KB message A→B over virtio under injected loss, checksum-verified.

### Phase N4 — Kernel RPC (KRPC)  ⬜
- [ ] N4.1 Versioned wire encoding + typed builder/cursor
- [ ] N4.2 Service/method registry with flags (IDEMPOTENT) + server worker pool
- [ ] N4.3 Client: correlation ids; blocking `krpc_call(node, method, req, resp, budget)` + async
- [ ] N4.4 At-most-once reply cache keyed (node, incarnation, call-id); peer-restart error
- [ ] N4.5 Deadline budget propagation + cancellation
- [ ] N4.6 Trace propagation + host tool stitching node logs by trace_id
- [ ] N4.7 Built-in services: `sys.ping`, `sys.echo`, `sys.info`
- [ ] N4.8 Tests: round trip, timeout, retry dedupe, concurrency, peer restart
- **Exit → M3:** `krpc_call(B, "sys.echo")` returns; a non-idempotent op runs exactly once under loss.

### Phase N5 — Surface, observability, demo  ⬜
- [ ] N5.1 `/net/peers`, `/net/stats` as devfs files
- [ ] N5.2 Per-layer counters + latency histograms (RTT p50/p99, retransmits, drops by reason)
- [ ] N5.3 Benchmarks: RPC RTT + bulk throughput baseline
- [ ] N5.4 Record M2/M3 demos; list Part III/IV hooks

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

**Immediate next action:** Part II, Phase **N0 — bench & building blocks**
(N0.2 `kthread_create`). Part I (A0–A3) is ✅ done and verified in QEMU;
buddy T0 (A0.5) is in place, its test sweep (A0.5d) still partial.

---

## 5. Change log
- 2026-09-07 — Initial master roadmap authored (post TTY Phase 6-multi).
- 2026-10-31 — Part I closed (A2, A3 ✅). Part II replaced with the detailed Phase 2 "Fabric" plan; decisions D1–D6 locked; corrected the nonexistent virtio-block claim.
