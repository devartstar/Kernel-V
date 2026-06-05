# DSSI 07 — Advanced Topics & The Global Operating System (Phases 23-28)

> **Scope:** Phases 23 through 28 — the six remaining phases that take Kernel-V from a working distributed OS to a *serious* one. This is the final document in the DSSI plan. After this, the only path forward is research papers, real users, and the next ten years.
>
> **Position in the master plan:** Part 8 of 8. Docs 00-06 built a complete DSSI: identity, observability, fabric, migration, automatic scheduling. Everything in Doc 07 is *amplification* — making the kernel faster (Phase 25 RDMA, Phase 26 CXL), smarter (Phase 23 advanced scheduling), more capable (Phase 24 distributed VFS), more trustworthy (Phase 27 security), and finally global-scale (Phase 28 WAN).
>
> **Honest framing:** this document is denser than its predecessors. Each of these phases could be a year-long project of its own. The descriptions here are *blueprints* sufficient to start, not full specifications. The user's 1-2 year window funds Doc 02-06 to high quality; Doc 07 is what year 2-4 looks like as the project matures. Some phases (especially 25, 26, 28) depend on hardware Kernel-V may not have access to during initial development — they're roadmap items, sketched here so the foundation we're building doesn't preclude them.

---

## 0. Thinking Process — Why These Six Phases, In This Order

### Why advanced scheduling before everything else

Phase 22 left us with `pick_victim` and `pick_target` as simple linear-score functions. Many real workloads don't fit that model:
- A database with two cooperating processes that must share a fast bus (affinity)
- A high-availability pair of replicas that must never co-locate (anti-affinity)
- A latency-sensitive media decoder with a 10ms deadline
- A nightly batch job that can run anywhere as long as it finishes by 6am
- A cluster trying to consolidate at night to power down idle nodes

Each is a *policy* the scheduler needs to express. Phase 23 turns the policy plug-in interface from Phase 20 into a *policy language*. This is a prerequisite for the next four phases because each of them (distributed VFS, RDMA, CXL, security) introduces new placement constraints. Without policy expressiveness, we'd hardcode each new constraint and watch the scheduler turn into spaghetti.

### Why distributed VFS before hardware acceleration

Phase 19 introduced the **deputy** mechanism so a migrated process could keep using home-node FDs via syscall proxying. That works but pins the resource to the home node forever. For workloads where the resource is the bottleneck — large databases, scientific datasets, build caches — the *file itself* needs to move. Phase 24 is what lifts that final tether.

Doing distributed VFS before RDMA matters because the VFS design constrains the network layer: if files can have multiple writers across nodes, we need cache-coherence protocols, distributed lock managers, write-invalidation messages. Designing the VFS first tells us *exactly* what the network has to deliver, so when we plug in RDMA in Phase 25 we know which patterns to optimize for.

### Why RDMA before CXL

RDMA is *software-visible network with hardware acceleration*. The fabric link becomes a zero-copy pipe, but the programming model is unchanged. We rewrite hot paths (page transfer for migration, page fetch for distributed VFS), measure the speedup, ship it.

CXL is *fundamentally new hardware* with a different programming model — load/store access to remote memory, cache-coherent across the bus. It changes what "remote memory" means at the architectural level. Building CXL support on top of mature RDMA-accelerated paths is straightforward; building both simultaneously means inheriting the bugs of both.

### Why security before WAN

The fabric until Phase 26 assumes a trusted LAN. Anyone with network reach can join. For a closet of nodes in your apartment, fine. For a global mesh on the public internet (Phase 28), catastrophic. Phase 27 makes the fabric **cryptographically authenticated and encrypted**, with capability-based authorization for cross-node operations. This is the prerequisite for opening up to untrusted networks.

Skipping Phase 27 and jumping to Phase 28 would be the single biggest mistake in the entire plan. The order is mandatory.

### Why the global mesh is last

Phase 28 is the most ambitious phase but also the most *amenable to being deferred*. The same fabric, with WAN-tuned timeouts and NAT traversal, can run across geographies. It's a refinement of everything that came before, not a new mechanism. Putting it last means every prior phase ships against the simpler LAN model, which is debuggable.

---

## 1. Phase 23 — Advanced Scheduling: Policies, Constraints, Goals

### 1.1 The Goal

Express the scheduler's behavior as **declarative constraints** instead of imperative code. An operator writes:

```
policy "production-db" {
  affinity:        "db-master + db-replica" on same_rack
  anti-affinity:   "db-master + db-master-backup" on different_node
  resource:        cpu >= 4, mem >= 8GB, disk_tier = ssd
  deadline:        none
  energy:          performance
}

policy "nightly-batch" {
  deadline:        complete_by 06:00
  energy:          consolidate
  preempt-by:      production-db
}
```

The scheduler reads these declarations and assigns processes to nodes satisfying all constraints.

### 1.2 The Constraint System

```c
typedef enum kv_constraint_kind {
    KVC_HARD_AFFINITY,        // must co-locate
    KVC_HARD_ANTI_AFFINITY,   // must NOT co-locate
    KVC_SOFT_AFFINITY,        // prefer to co-locate
    KVC_RESOURCE_MIN,         // cpu/mem/disk minimums
    KVC_DEADLINE,             // must complete by time T
    KVC_PREEMPT_BY,           // can be evicted for higher-priority policy
    KVC_LOCALITY,             // prefer rack/node group X
    KVC_ENERGY_HINT,          // performance | balanced | consolidate
} kv_constraint_kind_t;

typedef struct kv_constraint {
    kv_constraint_kind_t kind;
    union {
        struct { kv_gid_t a, b; } affinity;
        struct { uint32_t cpu_count; uint64_t mem_kb; } resource;
        struct { uint64_t deadline_ms; } deadline;
        struct { char policy_name[32]; } preempt;
    };
} kv_constraint_t;
```

The scheduler evaluates placements as constraint satisfaction: a placement is valid iff every hard constraint is satisfied; ranked by soft constraints; ordered by goal function (e.g., energy minimization).

This is the same approach Kubernetes uses (`PodAffinity`, `PodAntiAffinity`, `NodeSelector`) and that Borg's Alloc system pioneered. We're reusing well-tested ideas at the kernel layer instead of the userspace orchestrator layer.

### 1.3 The Two-Level Scheduler

- **Cluster-level scheduler** (this phase) decides *which node* a process runs on.
- **Per-node CPU scheduler** (already from Phase 4) decides *which CPU* and *when*.

The cluster scheduler runs per node, on the same 500ms tick from Phase 20. Each node makes local placement decisions using globally-gossiped state. Conflicts (two nodes both want to host the same migrating process) resolve via the optimistic accept-or-reject protocol from Phase 18.

There's deliberately **no central global scheduler**. The peer-to-peer model means there's no SPOF and no scalability ceiling. The cost is occasional sub-optimal placements; the win is correctness under partial failure.

### 1.4 Deadline-Aware Scheduling

A process can declare a deadline:

```c
struct kv_deadline {
    uint64_t deadline_wall_ms;     // absolute wall-clock deadline
    uint64_t estimated_work_ms;    // user-provided WCET estimate
    uint32_t priority_on_miss;     // KILL | BEST_EFFORT | NOTIFY_PARENT
};
```

The scheduler reserves CPU bandwidth: `deadline_wall_ms - now() >= estimated_work_ms * 1.3` (30% slack). If reservation impossible on any node, the process is queued or rejected with `EDEADLOCK`.

This is borrowed from Linux's SCHED_DEADLINE and from real-time OS literature. We don't pretend to be hard-real-time (kernel-V will never be that); we offer *soft* real-time with best-effort SLA.

### 1.5 Energy-Aware Scheduling

A fabric of 16 nodes may use 800W when fully spread, 250W when consolidated onto 4 nodes. For idle/nightly workloads, consolidation is huge.

```c
enum kv_energy_hint {
    KV_ENERGY_PERFORMANCE,    // never consolidate; latency matters
    KV_ENERGY_BALANCED,       // default; modest consolidation when safe
    KV_ENERGY_CONSOLIDATE,    // pack aggressively; suspend empty nodes
};
```

When `CONSOLIDATE` is dominant, the scheduler migrates processes off lightly-loaded nodes onto neighbors. Once a node is empty for 5 minutes, fabric supervisor issues a *soft suspend* (S3 sleep on the hardware). Gossip-aware peers know to skip the suspended node when looking for placement targets; the supervisor wakes it on demand.

Real-world impact: a closet with 8 always-on machines that idle most of the day can drop to 2 awake at night. ~75% energy reduction for that workload class.

### 1.6 Phase 23 Implementation Phasing (5 weeks)

| Week | Deliverable |
|------|-------------|
| 1 | Constraint type system, parser for policy DSL |
| 2 | Constraint evaluation, basic placement search |
| 3 | Affinity / anti-affinity / locality constraints |
| 4 | Deadline reservations, best-effort SLA enforcement |
| 5 | Energy hint + node suspend / wake; integration tests |

---

## 2. Phase 24 — Distributed VFS (Moving the Resource, Not Just the Process)

### 2.1 The Problem Phase 19 Left Open

A process opens `/big/dataset.bin` (10GB) on node-a. It migrates to node-b for CPU. Every read syscall now proxies back to node-a's disk — 200μs round-trip per syscall. For a workload that does 100,000 reads/sec, that's 20 *seconds* of latency overhead per second of work. Unusable.

Phase 24 lets the file itself move. After migration, the file (or its hot working set) can re-home on node-b, and reads go local.

This is the big leap from "stop-and-copy with deputies" to a *true* single system image. It's also where the most distributed-systems hardness lives.

### 2.2 The Design — Layered, Inspired by Plan 9

Three layers, bottom-up:

1. **Distributed Block Cache (DBC).** Pages of file content cached on whichever node accessed them recently. Coherence via invalidation messages.
2. **Distributed Inode Table (DIT).** File metadata (size, perms, mtime) cached and coherently invalidated like DBC.
3. **Distributed File Protocol (DFP).** A 9P-style request/response protocol between nodes. Open, read, write, lseek, close, stat all wire-callable.

A file has a **primary node** (initially the node it was created on) and zero or more **cache holders**. Writes are forwarded to the primary; the primary invalidates other caches; reads can be served from any holder.

This is Sprite filesystem design (Nelson, Welch, Ousterhout, 1988) with a 35-year refresh. It's also conceptually identical to how modern distributed databases (e.g., Spanner) handle data ownership.

### 2.3 Coherence Protocol

Standard MESI-style, applied to file pages:

| State | Meaning |
|-------|---------|
| **M** (Modified) | This node has the only valid copy; primary's copy is stale |
| **E** (Exclusive) | This node has the only cached copy; primary's copy is current |
| **S** (Shared) | Multiple nodes have read-only cached copies |
| **I** (Invalid) | No valid local copy |

Reads in S/E/M states serve from local cache (zero network). Reads in I state fetch from primary, become E. Writes in M write locally; writes in E upgrade to M; writes in S send invalidate-others to primary first.

The primary node maintains a small directory: per-page list of which nodes hold S copies. On a write, it sends `INVALIDATE` to each S holder, waits for ACKs, then grants M.

### 2.4 The Promotion Heuristic

The scheduler watches per-(process, file) access rates via TraceOS. When a process's reads from a file exceed K accesses/second for T seconds, and the process is not on the file's primary node, the scheduler considers **promoting** the file (changing its primary to the process's current node).

Promotion is the inverse of process migration: instead of moving the consumer to the resource, we move the resource to the consumer. Pick whichever is cheaper based on file size vs process size.

```c
bool should_promote_file(struct kv_proc *p, struct kv_file *f) {
    uint64_t process_size = p->resident_kb;
    uint64_t file_hot_set = f->stats.estimated_working_set_kb;

    if (f->primary_node == p->exec_node) return false;   // already here
    if (file_hot_set > process_size * 4) return false;   // moving the process is cheaper

    if (p->stats.read_rate_from(f->gid) > 1000) return true;

    return false;
}
```

Promotion involves: lock the file, flush dirty pages to old primary's stable storage, update directory to point to new primary, gossip the change. Bounded pause time per file: ~50ms for typical hot sets.

### 2.5 The Hard Part: Concurrent Writes

Single-writer-multiple-reader is easy. Multiple writers across nodes need careful coherence.

Two acceptable models:

- **Strict POSIX (`write` is atomic up to PIPE_BUF):** every write goes to primary, primary serializes. Slow but correct.
- **Relaxed (per-region writes don't race):** if writers write disjoint regions (typical for database log files), allow concurrent writes with byte-range locking on primary. Common case fast.

We implement both. Default is relaxed; strict mode is selectable per-file via xattr `kv.coherence=strict`.

This is exactly the tradeoff GFS, HDFS, and Lustre all wrestle with. We're not solving it more cleverly — we're letting the user pick per file.

### 2.6 Crash Recovery

Primary node death is the worst case. We need:

- **Replicated directory:** each file's "which nodes hold which states" directory is mirrored to a backup node, updated synchronously. (Cost: each cache-state transition becomes one extra message.)
- **Write-ahead log on primary:** dirty pages logged to local disk before being acknowledged to writer. After primary recovery, replay log.
- **Backup-promotion protocol:** if primary phi exceeds DEAD threshold, the directory backup promotes itself to primary. Holders find out via gossip. Ongoing in-flight writes get `EAGAIN` and retry against the new primary.

Recovery time: ~1-2 seconds for the failure-detect + promote sequence. During recovery, writes to that file block; reads from S-state caches continue.

### 2.7 Phase 24 Implementation Phasing (10 weeks)

| Week | Deliverable |
|------|-------------|
| 1-2 | DFP wire protocol; basic open/read/write/close over the fabric |
| 3-4 | Distributed Block Cache with MESI coherence (single primary, multi reader) |
| 5 | Multi-writer support with byte-range locking |
| 6 | Promotion heuristic and the promotion protocol |
| 7 | Distributed Inode Table |
| 8 | Replicated directory + write-ahead log on primary |
| 9 | Crash recovery: backup promotion, holder rediscovery |
| 10 | Performance tuning, integration with scheduler, full test suite |

This is the most complex phase in the entire plan. 10 weeks is a *minimum*; expect to spend longer.

---

## 3. Phase 25 — RDMA / RoCEv2 Transport

### 3.1 What RDMA Buys Us

Three things matter:

1. **Zero-copy:** the NIC reads/writes directly into application memory, bypassing kernel TCP/IP and userspace copies. CPU cost drops by 5-10×.
2. **Kernel bypass for I/O:** queue pairs let userspace post sends/receives directly to the NIC. Latency drops from ~10μs (TCP) to ~1μs (RoCEv2).
3. **One-sided operations:** RDMA READ and WRITE access remote memory *without* involving the remote CPU. This is the magic primitive.

For Kernel-V, the wins are concentrated:
- Migration page transfer: 100 MB/s (TCP) → 10 GB/s (RoCEv2)
- Distributed VFS page fetch: 200μs → 10μs
- Gossip and small messages: marginal (the latency floor is software stack, not network)

### 3.2 Hardware Reality

RDMA needs RDMA-capable NICs:
- **InfiniBand:** purpose-built, fastest, expensive, niche
- **RoCEv2:** RDMA over Converged Ethernet, runs on standard 25/40/100 GbE NICs with RDMA support (Mellanox/NVIDIA ConnectX-5+, Broadcom, Intel). **This is what we target.**
- **iWARP:** RDMA over TCP, more compatible but slower, dying

We add RDMA as an **optional acceleration**, not a replacement for the TCP fabric link. The fabric layer detects RDMA capability at HELLO handshake and uses it when both peers support it.

### 3.3 The Programming Model

Two queues per peer:
- **Send Queue (SQ):** post outbound work requests
- **Receive Queue (RQ):** pre-post buffers for inbound writes
- **Completion Queue (CQ):** poll for done items

Memory must be **registered** (pinned, with the NIC) before use. Registration is expensive (~10μs); we cache registrations in a pool.

For migration: register the source process's page set, post one giant RDMA READ from the target. Bytes move at line rate; CPU idles. Same for distributed VFS page fetch.

### 3.4 What We Build

```c
struct kv_rdma_link {
    struct ibv_qp *qp;            // queue pair
    struct ibv_cq *cq;
    struct kv_mr_pool *mr_pool;   // registered memory cache
    struct ibv_pd *pd;            // protection domain
    uint64_t remote_base;         // remote memory window we can read/write
    uint32_t remote_rkey;
};

ssize_t kv_rdma_read(struct kv_rdma_link *l, void *local_buf, size_t len, uint64_t remote_offset);
ssize_t kv_rdma_write(struct kv_rdma_link *l, void *local_buf, size_t len, uint64_t remote_offset);
```

Existing fabric code paths get a runtime branch: `if (peer->has_rdma && size > THRESHOLD) use_rdma() else use_tcp()`. THRESHOLD is typically 4KB — RDMA setup overhead dominates for tiny messages.

### 3.5 Phase 25 Implementation Phasing (5 weeks)

| Week | Deliverable |
|------|-------------|
| 1 | RDMA NIC driver bindings (use Linux's `libibverbs` API as model) |
| 2 | Queue pair lifecycle, MR pool, completion polling |
| 3 | Wire RDMA into migration page transfer; benchmark |
| 4 | Wire RDMA into distributed VFS page fetch |
| 5 | Fallback paths, integration tests, perf report |

---

## 4. Phase 26 — CXL Memory Disaggregation

### 4.1 What CXL Is

Compute Express Link (specification v3.0, published 2022) defines a CPU-to-device interconnect with three sub-protocols:

- **CXL.io**: PCIe-equivalent control plane (used for everything)
- **CXL.cache**: device-side cache coherent with CPU caches
- **CXL.mem**: load/store access to remote memory as if it were local

For a DSSI, CXL.mem is the big deal. A node with 64GB local DRAM and access to a 512GB CXL memory pool can run processes that "live" partly in remote memory, with the CPU's MMU handling the address translation. From the program's perspective, it just has 576GB of RAM.

CXL switches connect multiple hosts to shared memory pools. A 4-node DSSI fabric could share a pool: any process on any node sees the same memory region.

### 4.2 What's Real Today vs Future

As of early 2026:
- CXL 1.1/2.0 hardware shipping: yes, in datacenter products (Sapphire Rapids, Genoa, EMR)
- CXL 3.0 switches with multi-host pooling: emerging, limited availability
- Cache coherence across hosts (CXL.cache): rare in commodity hardware

Kernel-V should **support** CXL.mem (load/store to remote memory) on day one of Phase 26. CXL.cache cross-host coherence we treat as a research-grade optional feature.

### 4.3 Memory Tiers

Phase 26 introduces an explicit memory tier hierarchy:

| Tier | Latency | Bandwidth | Coherent? |
|------|---------|-----------|-----------|
| **L1** Local DRAM | ~100ns | ~100GB/s | yes (local) |
| **L2** Local CXL-attached | ~250ns | ~30GB/s | yes (local) |
| **L3** Pool CXL (same host group) | ~500ns | ~25GB/s | yes (CXL.mem) |
| **L4** Remote via RDMA | ~5μs | ~10GB/s | no (DSM-style) |
| **L5** Remote via TCP fabric | ~100μs | ~1GB/s | no (DSM-style) |

The kernel page allocator becomes tier-aware. Each VMA can pin to a tier or accept automatic placement. The page-replacement algorithm becomes a tier-promotion/demotion algorithm: hot pages move toward L1, cold pages move toward L5.

This is conceptually the same as the NUMA balancing Linux has done for 15 years, with a new layer of indirection (CXL) and a vastly wider gap between fastest and slowest.

### 4.4 Implementation Approach

We piggyback on Linux's CXL drivers (since Kernel-V is x86 and the CXL drivers are open-source). Initially: treat CXL.mem as just another NUMA node. Page tables work unchanged.

Then: integrate tier awareness into the page allocator. New VMA flag `KV_VMA_TIER_HINT`. New per-VMA counters (page faults per second, recent access histogram).

Then: cross-host pooled memory becomes a fabric primitive. A process can `mmap(MAP_FABRIC_POOL)` to get a region backed by the cluster's shared CXL pool. The kernel handles the coordination.

### 4.5 Phase 26 Implementation Phasing (6 weeks)

| Week | Deliverable |
|------|-------------|
| 1 | CXL device enumeration; treat as NUMA |
| 2 | Tier-aware page allocator; KV_VMA_TIER_HINT |
| 3 | Per-VMA access histograms; tier-promotion/demotion logic |
| 4 | MAP_FABRIC_POOL with single-host CXL pool |
| 5 | Cross-host pooled memory access (depends on CXL 3.0 switch availability) |
| 6 | Performance tuning, integration with migration (Phase 22) for tier-aware placement |

Phase 26 is *hardware-gated* in a way no prior phase is. Without CXL hardware, the first three weeks can still be implemented as a simulation; weeks 4-6 need real CXL or remain as design.

---

## 5. Phase 27 — Security, Trust, and Encryption

### 5.1 The Threat Model Shift

Through Phase 26, the threat model is: *honest nodes on a private network*. After Phase 28 (WAN), it's: *anyone on the public internet may try to join, lie about resources, intercept migrations, or impersonate peers*. Phase 27 bridges the two.

We assume:
- An attacker can sniff and inject network traffic
- An attacker may control some fraction of nodes (Byzantine)
- An attacker may try to migrate processes onto a node they control to extract data
- An attacker may try to claim resources they don't have to attract migrations
- Local kernel is trusted; userspace within a node is sandboxed by capabilities

We do not assume:
- An attacker has physical access to nodes (out of scope; use full-disk encryption + TPM if needed)
- Side-channel-class adversary (Spectre, Rowhammer): out of scope; standard mitigations apply

### 5.2 Cryptographic Node Identity

Every node generates an **Ed25519 keypair** at first boot. The public key becomes the *true* node identity; the UUID from Phase 16 is now derived from the public key (`uuid = SHA256(pubkey)[:16]`). Public keys are gossiped along with peer info.

The private key never leaves the node. It's stored in the node config block (Phase 16) — operators are expected to provide some form of secure storage (TPM-sealed, encrypted with operator passphrase, or accepted-as-clear-on-trusted-LAN).

### 5.3 Authenticated, Encrypted Fabric Link

Replace the cleartext TCP fabric link with a **Noise Protocol Framework** session (Noise_XX pattern: mutual authentication, forward secrecy, identity hiding from passive observers).

Handshake replaces Phase 16's HELLO:
```
A → B: e
B → A: e, ee, s, es
A → B: s, se
A ↔ B: encrypted KV_FMSG_HELLO with full identity claims
```

After handshake, every message is encrypted with AES-256-GCM with a unique per-message nonce. Replay protection via sequence numbers. The wire frame from Phase 16 unchanged; the *contents* are encrypted.

CPU cost: AES-NI hardware acceleration makes encryption essentially free on modern x86 (>10 GB/s per core). RDMA paths (Phase 25) need special care: data isn't encrypted in flight if it's RDMA-direct. For data-touching-only-trusted-nodes (LAN), this is fine. For WAN, we either tunnel RDMA through IPSec or fall back to encrypted TCP for cross-trust-boundary peers.

### 5.4 Capability-Based Cross-Node Authorization

A process or operator's authority to perform fabric operations (migrate, gossip-claim-resources, mount-distributed-file) is expressed as **signed capabilities**. A capability is a token saying "the bearer may do X with respect to Y," signed by the issuer.

```c
struct kv_capability {
    kv_node_pubkey_t issuer;        // who granted this
    kv_node_pubkey_t subject;       // who may use it
    uint32_t         operations;    // bitmask: MIGRATE, RECEIVE, MOUNT, etc.
    uint64_t         scope_pid_or_resource;
    uint64_t         expires_ms;
    uint8_t          signature[64]; // Ed25519
};
```

When node A asks node B to receive a migration of process P, A includes a capability proving "the operator of P authorized me to migrate it to B." B verifies the signature against the operator's public key (which is in the fabric's gossiped trust database).

This is the same model used by SPKI/SDSI, Macaroons, and more recently UCAN. It's well-trodden.

### 5.5 Trust Domains

Multiple organizations may share a fabric. Each gets a **trust domain**: a set of nodes whose public keys are in the same trust root.

- Processes only migrate within their trust domain by default.
- Cross-domain migration requires explicit operator-issued cross-domain capability.
- Gossip cross-domain is metadata-only (you see *that* a domain exists, not its resource details).

This enables a future "fabric marketplace" where companies sell idle CPU to each other across the public internet, without leaking workload data.

### 5.6 Attestation (Stretch Goal)

For paranoid workloads: before migrating a sensitive process to node B, A asks B to **attest** that it's running unmodified Kernel-V with a trusted bootchain. B produces a signed quote from its TPM (or Intel TXT, AMD SEV, ARM TrustZone) covering the boot measurements. A verifies before sending.

This is research-grade in 2026 but is *the* mechanism for "global compute marketplace" levels of trust. Phase 27 stubs the API; full implementation is for some future Phase 30+.

### 5.7 Phase 27 Implementation Phasing (6 weeks)

| Week | Deliverable |
|------|-------------|
| 1 | Ed25519 keypair generation, persistent key storage, pubkey-derived UUID |
| 2 | Noise XX handshake replacing HELLO; AES-GCM session crypto |
| 3 | Capability format, signing, verification; integration with migrate path |
| 4 | Trust domain model, gossip-level partitioning |
| 5 | Replay protection, key rotation protocol |
| 6 | Attestation API stub; security review; full test suite |

---

## 6. Phase 28 — Global Mesh: Kernel-V Across The Internet

### 6.1 The Vision Realized

Up through Phase 27, the fabric assumes datacenter-class networking: <1ms latency, >1Gbps bandwidth, packet loss <0.01%. Phase 28 makes the fabric work when those assumptions fail.

End state: a Kernel-V node in San Francisco can join a fabric with peers in Frankfurt, Tokyo, and São Paulo. Processes migrate across continents based on cost/proximity/availability. Latency-tolerant batch work moves to whichever region has the cheapest electricity right now. A laptop's process can pause when its battery dies, resume on a desktop that came online, then migrate to a server farm at night.

This is the user's original vision at its most expansive: **the world's compute as one operating system**.

### 6.2 What Has To Change

**Network transport:**
- TCP performs poorly on high-RTT lossy links. Replace with a QUIC-inspired transport: UDP-based, multiplexed streams, BBR-style congestion control, 0-RTT resume.
- This is a sizable subproject. Use a reference QUIC implementation (e.g., a port of `picoquic` or `quiche`) rather than write from scratch.

**NAT traversal:**
- Most nodes won't have public IPs. Use ICE-style hole punching with public STUN servers.
- For nodes behind symmetric NATs (~10% of cases), use a TURN relay — fabric can include "relay nodes" that volunteer bandwidth in exchange for fabric credits.

**Latency-aware everything:**
- Gossip intervals stretch for distant peers (200ms LAN → 5s WAN)
- Phi Accrual thresholds adjust per-peer based on measured RTT distribution
- Failure detection windows scale with latency
- Migration mode selection (Phase 22) heavily prefers stop-and-copy + heavy compression for WAN; pre-copy is too round-trip-sensitive

**Geographic locality awareness:**
- The peer table tags each peer with an estimated geographic region (derived from RTT triangulation or GeoIP)
- Scheduler constraints can include `region: us-west`, `latency_to: <50ms`, `cost_per_cpu_hour: <0.01`
- Process migration prefers nearby peers unless explicitly globally-routed

**Bandwidth budgets:**
- Operators set monthly bandwidth caps; fabric supervisor enforces and gossips remaining budget
- High-bandwidth operations (migration of large processes, VFS bulk transfer) negotiate quota before starting
- Inspired by libp2p's bandwidth shaping

**Routing optimization:**
- Direct A→B not always best. If A↔C and C↔B are fast but A↔B is slow, route fabric traffic through C.
- Use a gossip-built link-state map; recompute routes every minute
- The same algorithms that drive Anycast and CDN routing

### 6.3 What Stays The Same

Notably, *almost all the prior code is unchanged*. Phase 28 is a refinement of the network layer + scheduler policies, not a rebuild. The gossip protocol, migration mechanisms, deputizing, distributed VFS — all unchanged. They just run over a more capable transport with smarter timing.

This is the reward for the layered design: a global OS is reached by adding a transport and tuning constants, not by rewriting the kernel.

### 6.4 Phase 28 Implementation Phasing (8 weeks)

| Week | Deliverable |
|------|-------------|
| 1-2 | QUIC-inspired transport integration; coexists with TCP fabric link |
| 3 | NAT traversal: STUN client, ICE candidate gathering, hole punching |
| 4 | TURN relay support; fabric credit accounting |
| 5 | Per-peer latency-aware tuning (gossip rates, phi thresholds, migration mode bias) |
| 6 | Geographic tagging; region/latency scheduler constraints |
| 7 | Bandwidth budgets, quota negotiation |
| 8 | Inter-peer routing optimization; global demo: 4 nodes across 4 continents |

The final-week demo is the **payoff of the entire 24-month plan**: a single `fabric ps` command shows processes running on machines on four continents, migrations flowing between them, the fabric self-healing across submarine cable hiccups, the whole thing observable from any node.

---

## 7. Aggregate `KV_TRACE` Categories Added in Doc 07

```c
// Category: POLICY (0xA0) — Phase 23
#define KV_TRACE_POLICY_LOADED              0xA001
#define KV_TRACE_CONSTRAINT_EVALUATE        0xA002
#define KV_TRACE_PLACEMENT_FOUND            0xA003
#define KV_TRACE_PLACEMENT_FAILED           0xA004
#define KV_TRACE_DEADLINE_RESERVED          0xA010
#define KV_TRACE_ENERGY_CONSOLIDATE         0xA020
#define KV_TRACE_NODE_SUSPEND               0xA021
#define KV_TRACE_NODE_RESUME                0xA022

// Category: DVFS (0xB0) — Phase 24  (Distributed VFS, not CPU scaling)
#define KV_TRACE_DVFS_PAGE_FETCH            0xB001
#define KV_TRACE_DVFS_PAGE_INVALIDATE       0xB002
#define KV_TRACE_DVFS_FILE_PROMOTE          0xB010
#define KV_TRACE_DVFS_PRIMARY_FAILOVER      0xB020
#define KV_TRACE_DVFS_COHERENCE_TRANSITION  0xB030

// Category: RDMA (0xC0) — Phase 25
#define KV_TRACE_RDMA_QP_CREATE             0xC001
#define KV_TRACE_RDMA_MR_REGISTER           0xC002
#define KV_TRACE_RDMA_READ_COMPLETE         0xC010
#define KV_TRACE_RDMA_WRITE_COMPLETE        0xC011

// Category: CXL (0xD0) — Phase 26
#define KV_TRACE_CXL_TIER_PROMOTE           0xD001
#define KV_TRACE_CXL_TIER_DEMOTE            0xD002
#define KV_TRACE_CXL_POOL_ALLOC             0xD010

// Category: SECURITY (0xE0) — Phase 27
#define KV_TRACE_SEC_HANDSHAKE_START        0xE001
#define KV_TRACE_SEC_HANDSHAKE_OK           0xE002
#define KV_TRACE_SEC_HANDSHAKE_FAIL         0xE003
#define KV_TRACE_SEC_CAPABILITY_GRANTED     0xE010
#define KV_TRACE_SEC_CAPABILITY_REJECTED    0xE011
#define KV_TRACE_SEC_TRUST_DOMAIN_REJECT    0xE020

// Category: WAN (0xF0) — Phase 28
#define KV_TRACE_WAN_NAT_PUNCH              0xF001
#define KV_TRACE_WAN_RELAY_USED             0xF002
#define KV_TRACE_WAN_ROUTE_SELECTED         0xF010
#define KV_TRACE_WAN_REGION_LATENCY         0xF020
```

By Phase 28, the trace event taxonomy spans **15 categories and ~100 distinct event types**. The Kernel Doctor can explain causality from a userspace symptom all the way to "your nearest replica is in a network partition behind a relay that's congested."

---

## 8. Risks & Mitigations (Doc 07 Scale)

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Constraint solver becomes slow / NP-hard with many constraints | Medium | Bound search depth; backtracking with timeout; fall back to greedy |
| Distributed VFS coherence bugs corrupt data | High (complexity) | Extensive Jepsen-style randomized testing; per-file strict mode escape hatch |
| RDMA bypasses kernel security boundaries | Medium | Restrict RDMA-direct to within trust domain; encrypt or skip for cross-trust |
| CXL hardware unavailable to project | Certain in some phases | Simulation mode for development; real-hardware perf as future work |
| Cryptographic implementation bugs | High (always) | Use well-audited libraries (libsodium, BoringSSL); minimize custom crypto |
| Global routing wrong, processes migrate suboptimally | Medium | Conservative defaults; gradual rollout; operator visibility into route decisions |
| Bandwidth-cap accounting incorrect → unexpected bills | High (real money) | Hard local caps; reject operations that would exceed; clear telemetry |
| WAN failure modes uncovered until production | Certain | Chaos engineering test rig: simulate latency, loss, partitions continuously |

---

## 9. Full Timeline — All Eight Documents Integrated

| Doc | Phases | Weeks | Cumulative weeks |
|-----|--------|-------|------------------|
| 01 | 6-11 (foundation, baked into UNIX phases) | parallel with normal kernel work | — |
| 02 | 12-13 (observability) | 9 | 9 |
| 03 | 14-15 (slab + checkpoint) | 13 | 22 |
| 04 | 16-17 (fabric control plane) | 9 | 31 |
| 05 | 18-19 (first remote exec) | 11 | 42 |
| 06 | 20-22 (live migration & auto-scheduling) | 17 | 59 |
| 07 | 23-28 (advanced) | 40 | 99 |
| **Total** | | **99 weeks** | **~23 months** |

99 weeks = **just under 2 years**. Aligns exactly with the user's 1-2 year window for a *complete* DSSI delivery. Within 15 months (end of Doc 06), the user has a working DSSI with all major mechanisms operational. The remaining 8 months of Doc 07 add depth, hardware acceleration, security, and the global-mesh capstone.

---

## 10. Demo Milestone Summary (From Doc 00)

| Milestone | Document | "What you can show" |
|-----------|----------|---------------------|
| **M0** It pings | Doc 04 end | 4 nodes find each other, gossip resources |
| **M1** It moves | Doc 05 mid | Manual `migrate` works, deputy proxies syscalls |
| **M2** It moves itself | Doc 06 mid (Phase 20) | Auto-scheduler load-balances CPU-bound work across nodes |
| **M3** It moves fast | Doc 06 end (Phase 22) | Sub-30ms migration pause on real workloads |
| **M4** It moves files | Doc 07 mid (Phase 24) | Process migrates and the file follows it |
| **M5** It's fast | Doc 07 (Phase 25) | RDMA migration at 10GB/s |
| **M6** It's huge | Doc 07 (Phase 26) | Process uses 1TB memory pool across nodes |
| **M7** It's safe | Doc 07 (Phase 27) | Two organizations share a fabric without trust violations |
| **M8** It's global | Doc 07 end (Phase 28) | 4 nodes across 4 continents running one DSSI |

By M8, **Kernel-V is doing something no shipped OS has ever done**: a transparent, single-system-image kernel spanning data centers, with auto-migration, hardware-accelerated remote memory, and cryptographic cross-organization trust. It's not just a clone of MOSIX or Plan 9 — it's where those projects' visions were heading before they ran out of runway.

---

## 11. What Lies Beyond Phase 28

A short note on what comes *after* the 2-year plan. None of these are committed; they're directions:

- **Phase 29: Heterogeneous execution.** Migrate a process from x86 to ARM, lifting CPU architecture as a transparent property. Requires bytecode-level abstraction (WASM?), or per-arch binary versioning.
- **Phase 30: ML-driven scheduling.** Replace hand-tuned policies in Phase 23 with learned models that predict migration outcomes.
- **Phase 31: Containers as first-class.** Containers/sandboxes that span nodes natively; Docker-compatible runtime on top.
- **Phase 32: User-mode contributions.** Open the fabric to community-contributed schedulers, transports, and policies.
- **Phase 33: Edge/IoT mode.** A stripped-down Kernel-V that joins fabrics from microcontrollers (Raspberry Pi class) to share sensor data and offload compute.

Each is a year-scale project in its own right. The right time to plan them is when Phases 6-28 are operational and real users are pushing on the limits.

---

## 12. The Closing Argument

The user's original vision, paraphrased: *a management layer between userspace and kernelspace that transparently transfers process execution to other nodes when local CPU is saturated.*

After 8 documents and 99 weeks of plan, here is what we've built:

- **A kernel** (Phases 0-11) that is small, modern, and DSSI-aware from day one.
- **Eyes** (Phases 12-13) — comprehensive tracing and a diagnostic engine that can explain any distributed behavior in causal terms.
- **Frozen state** (Phases 14-15) — every process can become a portable binary blob.
- **A nervous system** (Phases 16-17) — nodes find each other, share their state, detect failures with sub-second resolution.
- **Movement** (Phases 18-19) — processes can migrate; they keep their identity at home and forward syscalls back.
- **Speed and intelligence** (Phases 20-22) — migration is sub-100ms, automatic, and adaptive.
- **Resource mobility** (Phase 24) — even the files can move.
- **Hardware acceleration** (Phases 25-26) — when the infrastructure supports it, we use it fully.
- **Trust** (Phase 27) — the fabric works across organizational boundaries with cryptographic certainty.
- **Reach** (Phase 28) — the fabric spans the planet.

What started as "transfer a process to another node" becomes a credible foundation for a *new* model of computing: one where the OS, not the operator, decides where work runs. Where adding a machine to your "computer" is as easy as joining a fabric. Where compute is fungible across the world the way electricity is fungible across a grid.

**This is the OS you set out to build.**

---

## 13. The Five Most Important Decisions in This Document

1. **Policy as data, not code.** Phase 23's constraint language means new requirements add declarations, not branches. The scheduler stays maintainable as it gets smarter.
2. **Distributed VFS uses MESI per-page.** Reusing a 40-year-old cache coherence protocol means we inherit decades of correctness analysis. New invented protocols would have new invented bugs.
3. **RDMA accelerates, never replaces.** TCP path always works. RDMA is detected, used when both ends support it, falls back silently otherwise. The kernel stays portable.
4. **CXL is a memory tier, not a new abstraction.** Treating CXL as "another NUMA level" lets all existing memory code work. The novelty is in the tier-aware allocator, not in pervasive changes.
5. **Security is mandatory before WAN.** Phase 27 before Phase 28 is non-negotiable. Untrusted networks demand cryptographic guarantees; LANs got away without them. Don't ship to the internet what you wrote for the closet.

---

## 14. What's Next

There is no **next document**. This is the eighth and final document in the DSSI plan.

What's next is **building it**. The plan is concrete enough to start writing code today against Phase 12 (TraceOS) while continuing the Phase 6-11 UNIX foundation work in parallel. Each phase's implementation phasing is a sequence of week-sized commits with verifiable results.

**Immediate next actions (the first 30 days):**
1. Finish Phase 5 (PCI discovery) cleanup if anything remains.
2. Begin Phase 12 (TraceOS) — the per-CPU ring buffers and the `KV_TRACE` macro. This is a 3-week investment that pays back for the entire remaining 96 weeks because every subsequent phase emits events through it.
3. Apply the Doc 01 *seeds* to ongoing Phase 6-11 work: `kv_gid_t` everywhere, serialize/deserialize stubs, async-first I/O.
4. Set up the multi-node QEMU test harness — even before fabric code exists, having `make qemu-4node` infrastructure speeds every later phase.

**Six-month checkpoint:** Phases 12-15 complete. Trace + ledger + slab + checkpoint working. You're ready to start Phase 16 with confidence that the foundation can support the fabric.

**Fifteen-month checkpoint:** Phases 16-22 complete. **You have a working DSSI.** Demo at conferences, open-source, write papers. This is the milestone that justifies the project.

**Twenty-three-month checkpoint:** Phase 28 complete. **Kernel-V is the world's first open, modern, single-system-image distributed kernel that scales from a closet to a continent.** This is the destination.

The blueprint is finished. The next 99 weeks are yours.
