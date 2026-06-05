# Kernel-V DSSI — Document 02: Observability (Phases 12-13)

> **TraceOS and the Resource Truth Ledger — through the lens of what the fabric needs.**
> A distributed system you cannot see is a distributed system you cannot debug. This document specifies the observability foundation that turns Kernel-V's fabric from "a black box that sometimes migrates processes" into "a system that explains every decision it makes."

This document covers **Phase 12 (TraceOS)** and **Phase 13 (Resource Ledger)** from your existing kernel plan — but specifies them with the fabric layer's needs baked in from day one.

---

## Table of Contents

1. [Why Observability is Existential for DSSI](#1-why-observability-is-existential-for-dssi)
2. [The Three Observability Subsystems](#2-the-three-observability-subsystems)
3. [TraceOS — Event Bus Architecture](#3-traceos--event-bus-architecture)
4. [Distributed Causal Tracing](#4-distributed-causal-tracing)
5. [Resource Truth Ledger](#5-resource-truth-ledger)
6. [Fabric-Specific Metrics](#6-fabric-specific-metrics)
7. [The Kernel Doctor](#7-the-kernel-doctor)
8. [Cutting-Edge Techniques](#8-cutting-edge-techniques)
9. [Implementation Phasing](#9-implementation-phasing)
10. [Tooling and Commands](#10-tooling-and-commands)
11. [Common Pitfalls](#11-common-pitfalls)
12. [Success Criteria](#12-success-criteria)

---

## 1. Why Observability is Existential for DSSI

Observability is non-negotiable for DSSI. Here is why:

### 1.1 You cannot debug what you cannot see

A migration bug in 2026 might look like: "the process resumed on Node C but its file descriptor table was missing fd #3." Without comprehensive tracing across both nodes, this bug is essentially undebuggable. You cannot attach gdb to a process that has moved. You cannot `printf` your way out of a race condition that spans two kernels.

The trace must be the **primary debugging interface** for fabric. Logs are secondary.

### 1.2 The fabric scheduler needs truth, not approximations

Every migration decision the fabric makes — "should I send this process to Node B or Node C?" — depends on **accurate, recent, fine-grained resource data**. Without a real resource ledger:

- The fabric guesses → bad migration → worse performance than no migration at all
- "Node C looks idle" is meaningless if you don't know *why* (cold cache? warming up? sleeping?)

The resource ledger is the *substrate* the fabric scheduler reasons over.

### 1.3 Causal debugging is the only way to reason about distributed bugs

A user reports: "my process ran slow." Without causal tracing, you have:
- A pile of log lines from Node A
- A pile of log lines from Node B  
- A pile of log lines from Node C
- No way to correlate which lines pertain to *this process's* execution

With causal tracing, you have:
- A single timeline of every event in this process's life, across all nodes it touched
- Parent-child links showing which event caused which
- Latency attribution: which span contributed most to total runtime

This is the **distributed tracing model** (think Jaeger, Zipkin, OpenTelemetry) brought into the kernel itself.

### 1.4 Self-explanation is your project's identity

Your existing `kernel_plan.md` makes this the centerpiece: "A UNIX-like kernel that explains itself." DSSI must extend this property to the fabric. A migration must be explainable: "I migrated process 42 to Node C because Node A's CPU was 95% utilized and Node C's was 12%, and the process's working set was estimated at 8 MB which transfers in 5 ms over the fabric link."

Without observability, you have black-box migrations. With it, you have a research kernel people can learn from.

---

## 2. The Three Observability Subsystems

Three distinct subsystems compose Kernel-V's observability:

```
┌─────────────────────────────────────────────────────────────┐
│  TRACEOS                                                     │
│  Event bus. Structured records of "something happened."     │
│  High-frequency, ring-buffered, with causal links.          │
│  Examples: syscall entered, page faulted, process migrated. │
├─────────────────────────────────────────────────────────────┤
│  RESOURCE LEDGER                                             │
│  Time-series metrics. Aggregates of "how much / how often." │
│  Per-process, per-device, per-node, per-fabric.             │
│  Examples: CPU ticks consumed, bytes read, IOPS.            │
├─────────────────────────────────────────────────────────────┤
│  KERNEL DOCTOR                                               │
│  Diagnostic engine. Reads from both above, applies rules.   │
│  Outputs human-readable diagnoses.                          │
│  Example: "process 5 is starved on block I/O queue depth."  │
└─────────────────────────────────────────────────────────────┘
```

These are **layered**: the doctor reads from the ledger and trace; the ledger may be derived from aggregated traces.

---

## 3. TraceOS — Event Bus Architecture

### 3.1 The event record

Every trace event is a fixed-size record (variable payloads go in a side buffer):

```c
typedef struct kv_trace_event {
    uint64_t   event_id;          // monotonic per node — globally unique with (node_id, event_id)
    uint64_t   parent_event_id;   // causal parent (0 if root)
    uint64_t   trace_id;          // groups events belonging to same logical operation
    uint64_t   timestamp_tsc;     // CPU time-stamp counter at emit
    uint32_t   node_id;           // emitter node (for fabric correlation)
    uint16_t   category;          // KV_TRACE_PROC, KV_TRACE_BLOCK, etc.
    uint16_t   type;              // specific event within category
    uint32_t   subject_gid_lo;    // local id of the primary subject (pid, fd, etc.)
    uint32_t   subject_gid_node;  // node_id of subject (0=local)
    uint32_t   arg0;
    uint32_t   arg1;
    uint32_t   arg2;
    uint32_t   payload_offset;    // 0 if no payload; else offset into payload ring
    uint32_t   payload_len;
} kv_trace_event_t;   // exactly 64 bytes — one cache line
```

**Why fixed 64 bytes:**
- Fits in one cache line — atomic single-write
- Aligns with hardware prefetchers
- Predictable ring buffer math (no variable-length entries to skip)

### 3.2 The category/type taxonomy

Categories are 16-bit; types are 16-bit. Stable namespace:

```c
// kernel/include/kv_trace_types.h

// Categories (high bits)
#define KV_TC_BOOT     0x0001
#define KV_TC_PROC     0x0010
#define KV_TC_MEM      0x0020
#define KV_TC_VFS      0x0030
#define KV_TC_BLOCK    0x0040
#define KV_TC_IPC      0x0050
#define KV_TC_NET      0x0060
#define KV_TC_PCI      0x0070
#define KV_TC_DRIVER   0x0080
#define KV_TC_SYSCALL  0x0090
#define KV_TC_INTR     0x00A0
#define KV_TC_SCHED    0x00B0
#define KV_TC_FABRIC   0x1000   // reserved for fabric, all subtypes
#define KV_TC_LEDGER   0x1100   // ledger sampling events

// Example types (one per significant event)
#define KV_TE_PROC_CREATE        (KV_TC_PROC | 0x01)
#define KV_TE_PROC_EXIT          (KV_TC_PROC | 0x02)
#define KV_TE_PROC_CTX_SWITCH    (KV_TC_PROC | 0x03)
#define KV_TE_PROC_MIGRATE_BEGIN (KV_TC_PROC | 0x04)
#define KV_TE_PROC_MIGRATE_END   (KV_TC_PROC | 0x05)

#define KV_TE_MEM_PAGE_FAULT     (KV_TC_MEM  | 0x01)
#define KV_TE_MEM_PAGE_MAPPED    (KV_TC_MEM  | 0x02)
#define KV_TE_MEM_PAGE_FREED     (KV_TC_MEM  | 0x03)

#define KV_TE_FABRIC_PEER_UP     (KV_TC_FABRIC | 0x01)
#define KV_TE_FABRIC_PEER_DOWN   (KV_TC_FABRIC | 0x02)
#define KV_TE_FABRIC_GOSSIP_TX   (KV_TC_FABRIC | 0x03)
#define KV_TE_FABRIC_GOSSIP_RX   (KV_TC_FABRIC | 0x04)
#define KV_TE_FABRIC_MIG_DECIDE  (KV_TC_FABRIC | 0x05)
#define KV_TE_FABRIC_PAGE_FETCH  (KV_TC_FABRIC | 0x06)
#define KV_TE_FABRIC_SYSC_PROXY  (KV_TC_FABRIC | 0x07)
```

**The rule:** every category gets its own 0x10 range. Within a category, type 0x00 is reserved (meaning "unknown / generic"). Types never get reused even after deletion (just like syscall numbers — see Document 01 Seed 5.2).

### 3.3 The emission macro

The single API everything uses:

```c
// kernel/include/kv_trace.h
extern bool kv_trace_enabled;

#define KV_TRACE(type, subj_gid, a0, a1, a2)                    \
    do {                                                         \
        if (__predict_false(kv_trace_enabled)) {                 \
            kv_trace_emit_(type, subj_gid, a0, a1, a2);          \
        }                                                        \
    } while (0)

void kv_trace_emit_(uint16_t type, kv_gid_t subj,
                    uint32_t a0, uint32_t a1, uint32_t a2);
```

**Critical performance properties:**
- One branch check when tracing is off: ~1-2 cycles
- When tracing is on: ~30-50 cycles for the emit (one cache-line write to ring, one timestamp read)
- No allocation, no lock contention (per-CPU rings, see below)
- Can be called from any context: process, interrupt, scheduler

The `__predict_false` hint makes the off-path the cold path. With tracing off (rare in development, common in production), the cost is negligible.

### 3.4 Per-CPU ring buffers (lockless emission)

Each CPU has its own ring buffer for trace events. Emission is wait-free:

```c
typedef struct kv_trace_ring {
    kv_trace_event_t *entries;       // fixed-size circular array
    uint32_t          size;          // power of 2
    uint32_t          mask;          // size - 1
    atomic_t          head;          // producer (kernel)
    atomic_t          tail;          // consumer (reader process)
    uint8_t          *payload_pool;  // variable payloads
    atomic_t          payload_head;
} kv_trace_ring_t;

// Per-CPU. On single-CPU kernel, one ring globally; SMP later.
extern kv_trace_ring_t *kv_trace_rings;   // [num_cpus]
```

**Lockless emit:** Producer uses `atomic_fetch_add(&head, 1)` to claim a slot, writes the event, done. Consumer (reader) reads from `tail` and advances. If producer outruns consumer, oldest events are overwritten — this is fine; we prefer dropping old events over blocking.

**LTTng inspiration:** This is the same design pattern as Linux's LTTng (Linux Trace Toolkit Next Generation). Battle-tested at multi-GHz event rates.

### 3.5 Variable payloads in side buffer

Sometimes you want to attach a string, a struct, or a small buffer to an event. The 64-byte record can't hold it. Solution: a separate payload pool with offset/length references.

```c
// At emit time, if you have variable payload:
uint32_t off = kv_trace_payload_alloc(len);
memcpy(payload_pool + off, ptr, len);
KV_TRACE_WITH_PAYLOAD(type, subj, a0, a1, a2, off, len);
```

Payload pool is also ring-allocated and can be overwritten. Readers that get stale offsets simply skip.

### 3.6 Trace filters and sampling

Always-on tracing is expensive in volume. Provide filtering:

```c
typedef struct kv_trace_filter {
    uint32_t  category_mask;     // bitmap of enabled categories
    uint32_t  sample_1_in_n;     // 1 = every event; 100 = every 100th
    kv_gid_t  subject_filter;    // 0 = all; else only events about this subject
    uint64_t  min_timestamp;     // events before this are dropped
} kv_trace_filter_t;

void kv_trace_set_filter(const kv_trace_filter_t *);
```

**Use cases:**
- Production: only `KV_TC_FABRIC` and `KV_TC_PROC`, sampled 1-in-1000
- Debugging migration: full `KV_TC_FABRIC` plus filtered to one pid
- Performance profiling: every event for 1 second, then disable

### 3.7 Trace dump format

The trace can be dumped to a file or transferred over the fabric in two formats:

**Compact binary** (default):
- Raw event records, prepended with a manifest (node_id, TSC frequency, start timestamp)
- ~64 bytes per event; 100 MB holds ~1.5M events
- Used for inter-node trace shipping

**Human-readable text:**
- One event per line, decoded fields
- For console inspection
- Format: `[ts] node=N pid=N:N type=PROC_CTX_SWITCH parent_event=N args=(...)`

Provide a `trace_decoder` user tool that converts binary → text and produces flame-graph-style visualizations.

---

## 4. Distributed Causal Tracing

This is the most important section of this document for DSSI.

### 4.1 The problem

A process spawned on Node A migrates to Node C, runs a syscall, the syscall proxies back to Node A, returns, the process completes. To debug this, you need *one timeline* spanning Node A → Node C → Node A → Node C. The events live in two ring buffers on two machines. How do you correlate them?

### 4.2 Inspiration: W3C Trace Context

The web/microservices world solved this. Every HTTP request carries a `traceparent` header containing:
- A `trace_id` (128 bits) — same for every span in the trace
- A `span_id` (64 bits) — unique per operation within the trace
- A `parent_span_id` (64 bits) — the span that caused this one

Every span recorded by every service shares the trace_id, so you can reconstruct the timeline by simple query: "give me all events with trace_id=X."

### 4.3 Kernel-V's adaptation

Every trace event already has:
- `trace_id` (64 bits — we're space-constrained vs 128)
- `event_id` (acts as span_id)
- `parent_event_id` (acts as parent_span_id)

**The rules:**
- When a process is *created*, it gets a fresh `trace_id`. Stored on the `proc_t`.
- Every event emitted in the context of that process inherits the `trace_id`.
- When the process forks, the child gets the *same* trace_id (it's part of the same logical operation tree).
- When a process migrates, the trace_id travels with it. Events on Node C are tagged with the same trace_id as events on Node A.
- When fabric messages cross the wire, the message header carries the trace_id of the operation that caused it.

```c
typedef struct proc {
    // ... existing ...
    uint64_t trace_id;          // shared across migrations, forks
    uint64_t last_event_id;     // for chaining causal children
} proc_t;
```

### 4.4 Implicit parent tracking via "current event"

You don't want every caller to explicitly pass `parent_event_id`. Instead: per-CPU "current event" cursor.

```c
// Per-CPU
static __thread uint64_t current_event_id;

void kv_trace_emit_(...) {
    event.event_id = atomic_inc(&global_event_seq);
    event.parent_event_id = current_event_id;   // automatic causal chain
    event.trace_id = current->trace_id;
    // ... write ...
    current_event_id = event.event_id;          // chain forward
}
```

**The result:** All events emitted during a syscall automatically chain back to the SYSCALL_ENTER event. All events during an interrupt handler chain to the INTR_ENTER event. Causal structure is free.

### 4.5 Cross-node correlation

When fabric sends a message to a peer, the message header includes:

```c
typedef struct kv_fabric_msg_hdr {
    uint32_t   magic;
    uint16_t   version;
    uint16_t   type;
    uint64_t   trace_id;        // <-- propagates trace
    uint64_t   parent_event_id; // <-- causal parent on sender
    uint64_t   src_node_id;
    uint64_t   dst_node_id;
    uint32_t   payload_len;
    uint32_t   checksum;
} kv_fabric_msg_hdr_t;
```

When the receiver processes the message, it sets `current_event_id = parent_event_id` and continues emitting events in the same causal chain. The trace_id makes correlation trivial: collect events from all nodes with trace_id=X, sort by timestamp, you have the timeline.

### 4.6 The TSC clock-skew problem

Each node's TSC starts at boot and runs at its own (slightly different) frequency. Comparing timestamps across nodes naively gives nonsense ("event B happened before event A even though A caused B").

**Solution (Phase 17+):** Lightweight clock sync. NTP is overkill. Use a simpler scheme:
- Each fabric message includes the sender's TSC value
- Receiver records local TSC at receive
- Build a per-peer offset estimate (exponentially-weighted moving average)
- Convert all timestamps to a "fabric time" using sender's TSC + estimated offset

Alternative: **PTP (Precision Time Protocol)** if the network supports it — gives sub-microsecond sync.

For TraceOS, present three timestamps per event when shown across nodes:
- Local TSC (raw)
- Local wall time (when known)
- Fabric time (offset-corrected, monotonic across nodes)

### 4.7 Flame graphs from causal traces

Once you have causal parent-child links, a flame graph is one Python script away. Each span's "self time" = its duration minus children's duration. Visualize as horizontal bars stacked vertically.

This is your **single most powerful debugging tool**. When a migration takes 200ms, the flame graph tells you exactly which 137ms was spent in `page_fetch_remote` and which 43ms was spent in `proc_restore`. No more guessing.

---

## 5. Resource Truth Ledger

### 5.1 The principle: every resource is counted

The ledger answers, at any moment, for any process or device or node:
- How much CPU has been consumed?
- How many bytes read/written?
- How many page faults?
- How long blocked on each resource type?

These counts are the **substrate** for:
- Phase 14 storage-aware scheduler
- Phase 17 fabric resource advertisement
- Phase 23 distributed scheduler decisions
- Phase 25+ fabric-wide accounting

### 5.2 Per-process ledger

Every process has a ledger struct:

```c
typedef struct kv_proc_ledger {
    // CPU
    atomic64_t cpu_ticks_user;
    atomic64_t cpu_ticks_kernel;
    atomic64_t cpu_migrations;          // times this proc moved between CPUs
    atomic64_t fabric_migrations;       // times this proc moved between nodes

    // Memory
    atomic64_t pages_allocated;
    atomic64_t pages_freed;
    atomic64_t page_faults_minor;       // satisfied without I/O
    atomic64_t page_faults_major;       // needed I/O
    atomic64_t pages_remote_fetched;    // fabric: fetched from another node
    atomic64_t peak_rss_pages;

    // I/O
    atomic64_t bytes_read_local;
    atomic64_t bytes_read_remote;       // fabric: read over network
    atomic64_t bytes_written_local;
    atomic64_t bytes_written_remote;
    atomic64_t block_io_count;
    atomic64_t block_io_time_ticks;     // total ticks spent waiting on block I/O

    // Syscalls
    atomic64_t syscalls_total;
    atomic64_t syscalls_proxied;        // fabric: forwarded to home node
    atomic64_t syscall_time_ticks;

    // IPC
    atomic64_t pipe_bytes;
    atomic64_t signals_received;
    atomic64_t signals_sent;

    // Scheduler
    atomic64_t scheduler_invocations;
    atomic64_t time_runnable_ticks;     // total time in RUNNABLE state
    atomic64_t time_running_ticks;
    atomic64_t time_blocked_ticks;

    // Fabric (zero until fabric exists)
    atomic64_t fabric_bytes_sent;       // total fabric traffic on this proc's behalf
    atomic64_t fabric_bytes_received;
    atomic64_t fabric_rpc_count;
} kv_proc_ledger_t;
```

**Update sites:** Every metric increments at exactly one place in the kernel (no double-counting). All increments are `atomic` so SMP-safe in the future.

**Cost:** ~100 atomic counters × 8 bytes = 800 bytes per process. Trivial.

### 5.3 Per-device ledger

Each `kv_device_t` has its own counters (Seed 2.3 in Document 01 introduced this):

```c
typedef struct kv_device_ledger {
    atomic64_t bytes_read;
    atomic64_t bytes_written;
    atomic64_t ops_count;
    atomic64_t errors;
    atomic64_t queue_depth_current;
    atomic64_t queue_depth_peak;
    // Latency histogram (see 5.5)
    kv_hdr_histogram_t latency_us;
} kv_device_ledger_t;
```

### 5.4 Per-node aggregates

Some metrics live at the node level, not per-process:

```c
typedef struct kv_node_ledger {
    // CPU
    atomic64_t total_cpu_ticks;
    atomic64_t idle_cpu_ticks;
    atomic64_t irq_ticks;
    atomic64_t kernel_ticks;
    atomic64_t user_ticks;

    // Memory
    atomic64_t total_pages;
    atomic64_t free_pages;
    atomic64_t cached_pages;
    atomic64_t kernel_pages;
    atomic64_t page_table_pages;

    // I/O aggregate
    atomic64_t block_io_total;
    atomic64_t network_packets_rx;
    atomic64_t network_packets_tx;
    atomic64_t network_bytes_rx;
    atomic64_t network_bytes_tx;

    // Fabric
    atomic64_t fabric_messages_sent;
    atomic64_t fabric_messages_received;
    atomic64_t fabric_active_migrations;
    atomic64_t fabric_total_migrations;

    // Health
    uint64_t   boot_timestamp;
    uint64_t   last_update_timestamp;
    uint32_t   load_avg_1min_x1000;     // load average × 1000 (no float)
    uint32_t   load_avg_5min_x1000;
} kv_node_ledger_t;
```

This is exactly the data the fabric gossips to peers (Phase 17). It is also what the fabric scheduler reads to make decisions.

### 5.5 HDR Histograms for latencies

Latency is a distribution, not a number. "Average block I/O latency = 5 ms" is useless if half of operations take 10 µs and half take 10 ms. The fabric scheduler needs to know **tail latencies**: p50, p95, p99, p99.9.

**Solution: HDR Histogram** (High Dynamic Range Histogram, by Gil Tene). Records every value into log-scale buckets; gives O(1) insert and O(buckets) percentile lookup. Standard in production observability stacks.

```c
typedef struct kv_hdr_histogram {
    uint64_t  bucket_counts[N_BUCKETS];   // ~600 buckets covers 1ns..1hour
    uint64_t  count_total;
    uint64_t  sum;                        // for mean
} kv_hdr_histogram_t;

void   kv_hdr_record(kv_hdr_histogram_t *, uint64_t value);
uint64_t kv_hdr_percentile(kv_hdr_histogram_t *, double p);  // p in 0..100
```

Apply to:
- Block I/O latency per device
- Page-fault servicing latency
- Syscall latency per syscall number
- Fabric message round-trip time per peer
- Scheduler decision latency

### 5.6 Time-series snapshotting

The ledger is a moving target. To answer "what was the load 5 seconds ago?", you need historical snapshots.

Solution: **Ring of snapshots**. A periodic timer (e.g., every 100 ms) copies the current `kv_node_ledger_t` into a ring of N snapshots. Most-recent N seconds of history always available.

```c
#define LEDGER_SNAPSHOT_INTERVAL_MS  100
#define LEDGER_SNAPSHOT_HISTORY      600    // 60 seconds at 100ms

typedef struct kv_ledger_snapshot {
    uint64_t timestamp;
    kv_node_ledger_t snapshot;
} kv_ledger_snapshot_t;

extern kv_ledger_snapshot_t ledger_history[LEDGER_SNAPSHOT_HISTORY];
extern atomic_t ledger_history_head;
```

Compute first/second derivatives from this: "CPU usage is rising 5% per second" → triggers proactive migration before saturation.

---

## 6. Fabric-Specific Metrics

These are metrics that exist *only* once fabric is online (Phase 17+) but should be **reserved in the data structures now**:

### 6.1 Per-peer metrics

For each known fabric peer, track:

```c
typedef struct kv_peer_metrics {
    kv_node_id_t peer;

    // Connectivity
    uint64_t       last_seen_tsc;
    uint64_t       last_gossip_received_tsc;
    uint32_t       gossip_received_count;
    uint32_t       gossip_sent_count;

    // Failure detection (Phi Accrual — Phase 17)
    double         phi_suspicion_level;  // higher = more suspicious

    // Performance
    kv_hdr_histogram_t rtt_us;           // round-trip time to this peer
    uint64_t       bytes_sent_to;
    uint64_t       bytes_received_from;
    uint32_t       active_migrations_to;
    uint32_t       active_migrations_from;

    // Trust (Phase 27+)
    uint32_t       trust_level;          // 0=untrusted, 100=fully trusted
    uint64_t       last_auth_success;
} kv_peer_metrics_t;
```

### 6.2 Per-migration metrics

Every migration emits a structured record at completion:

```c
typedef struct kv_migration_record {
    kv_gid_t       proc;
    kv_node_id_t   from_node;
    kv_node_id_t   to_node;
    uint64_t       begin_tsc;
    uint64_t       end_tsc;
    uint64_t       downtime_us;          // user-visible pause
    uint64_t       bytes_transferred;
    uint64_t       compression_ratio_x1000;  // ratio × 1000 (e.g., 2500 = 2.5x)
    uint32_t       pages_transferred;
    uint32_t       pages_dirtied_during;
    uint32_t       iterations;           // for pre-copy
    uint32_t       reason;               // MIG_REASON_CPU_PRESSURE, etc.
    int32_t        result;               // 0 success, else error
} kv_migration_record_t;
```

Keep a ring of the last N migrations. Doctor analyzes patterns ("migrations to Node C take 3x longer than to Node B — investigate").

### 6.3 Fabric SLO tracking

Define and measure Service-Level Objectives for the fabric itself:

```c
typedef struct kv_fabric_slos {
    // Migration
    uint64_t  migration_downtime_p99_us;   // target: < 50,000 (50ms)
    uint64_t  migration_total_p99_us;      // target: < 5,000,000 (5s)

    // Gossip
    uint64_t  gossip_freshness_p99_ms;     // target: < 1000 (1s)

    // Syscall proxy
    uint64_t  proxy_latency_overhead_p99_us;  // target: < 200

    // Availability
    uint32_t  fabric_partitions_per_hour;
    uint32_t  failed_migrations_per_100;
} kv_fabric_slos_t;
```

The doctor flags when SLOs are breached.

---

## 7. The Kernel Doctor

### 7.1 What it is

A diagnostic engine that combines the ledger and trace to produce **human-readable** diagnoses. Inspired by Linux's `perf top`, `dmesg`, and the unloved-but-brilliant `systemtap`.

Run as:

```sh
> doctor
```

Output:

```
Kernel-V Doctor Report — Node 7 — Fri 14:23:08
─────────────────────────────────────────────────────────
HEALTH: Degraded

Findings:
  [WARN] pid=0:42 /bin/render — high page fault rate
         137 minor faults/sec, 5 major faults/sec sustained
         likely cause: working set exceeds physical memory
         related events: trace_id=8a4c (last 30 events)
         suggested action: increase RAM or trigger fabric migration

  [WARN] block device nvme0 — queue depth saturated
         current depth: 31/32, p99 latency 14ms (target: <5ms)
         dominant writer: pid=0:42 (89% of writes)
         related: pid=0:11 blocked 4.2s on nvme0 reads
         suggested action: throttle pid=0:42 with `storage throttle`

  [INFO] fabric — peer node=3 high RTT
         RTT p95: 12ms (peer-average: 1.2ms)
         likely cause: peer node 3 may be on wireless
         no automatic action

  [OK]   cpu utilization: 67%, memory free 1.2GB, fabric: 4 peers up
```

### 7.2 How it works

The doctor is a set of **rule modules**. Each module:
- Subscribes to certain trace events or polls certain ledger metrics
- Maintains rolling-window state
- Emits findings with severity (OK / INFO / WARN / CRITICAL)

```c
typedef struct kv_doctor_finding {
    uint32_t  severity;
    char      summary[128];
    char      detail[512];
    char      suggested_action[256];
    uint64_t  related_trace_id;    // for cross-reference
} kv_doctor_finding_t;

typedef struct kv_doctor_rule {
    const char *name;
    void      (*check)(struct kv_doctor_rule *, kv_doctor_finding_t *out, size_t *n);
    void       *private;
} kv_doctor_rule_t;
```

### 7.3 Built-in rules (a starter set)

| Rule | What it checks | When it fires |
|------|---------------|---------------|
| `cpu_starvation` | Per-process runnable-but-not-running time | A proc waits > 100ms while CPU idle exists |
| `memory_pressure` | Free pages, page fault rates | Free < 5% OR major faults > 10/s |
| `block_queue_depth` | Per-device queue saturation | Depth > 90% for 5+ seconds |
| `tail_latency_anomaly` | Per-device p99 latency | p99 > 5× p50 |
| `fabric_peer_drift` | TSC offset to peer | Offset growing > 1ms/s |
| `migration_failure_rate` | Migration success/fail | < 95% success in last 10 |
| `lock_contention` | Time spent waiting on each lock | Top lock > 30% of contention |
| `dirty_page_buildup` | Pages dirtied but not flushed | > 1000 dirty pages > 10s |

Adding a rule is ~100 lines of C. **Encourage your future self (or contributors) to write rules** — every distributed-system bug you debug should become a rule that prevents it next time.

### 7.4 The "explain" command

A complement to the doctor: `explain` answers ad-hoc "why?" questions.

```sh
> explain pid 0:42
Process pid=0:42 /bin/render
  Started on this node at boot+128.4s
  Trace ID 8a4c1f
  Current state: RUNNABLE (waiting for CPU)
  Has been runnable for 47ms

  Resource usage (last 1s):
    CPU: 92% of one core
    Memory: 124MB resident, 8MB working set
    Block I/O: 1.2 MB read, 0 written
    Fabric: not migrated, no fabric traffic

  Recent significant events:
    +0.000s SYSCALL_ENTER read fd=3
    +0.001s BLOCK_IO_SUBMIT lba=15820 count=8
    +0.014s BLOCK_IO_COMPLETE  (13ms — above device p99 of 4ms!)
    +0.014s SYSCALL_EXIT 4096
    [...]

  Doctor suggests: ./doctor's memory_pressure rule is monitoring this process

> explain migration latest
Migration #41: pid=0:8 /bin/build
  From: node=0  To: node=3
  Triggered: 2025-12-04 14:22:18.418
  Reason: CPU_PRESSURE (local node 91%, peer node 14%)
  Duration: 1.42s total, 8ms downtime
  Bytes: 47 MB transferred, 3.1x compression ratio
  Iterations: 3 pre-copy passes
  Result: SUCCESS
  Trace ID: 4c9a02 (use 'trace show 4c9a02' for timeline)
```

This is the user-facing "kernel as live textbook" feature. It is also the demo that makes everyone watching say "I want this."

---

## 8. Cutting-Edge Techniques

Techniques worth studying and adopting:

### 8.1 eBPF-style attachable probes (deferred)

Linux's eBPF lets users attach safe probes that run in the kernel at trace points. Kernel-V is too early for a verifier and a JIT, but **the trace-point architecture** (named, stable, low-cost when no probe attached) is exactly the right shape. Build trace points so future-you can add a probe interpreter (Phase 25+) without rearchitecting.

### 8.2 LTTng-style lockless ring buffer

Already covered in 3.4. Worth restating: studying the LTTng paper (Mathieu Desnoyers, "The LTTng tracer: a low impact performance and behavior monitor for GNU/Linux") will save weeks.

### 8.3 Magic Pocket / Honeycomb-style sampling

In production, full event capture is impossibly expensive. Honeycomb (and Dropbox's internal Magic Pocket) use **tail-based sampling**: every span is buffered briefly; if anything in the trace was anomalous (high latency, error), keep the whole trace; otherwise sample 1-in-1000. Apply this to fabric: keep every trace of a slow migration, sample everything else.

### 8.4 Causal Profiling (COZ)

A 2015 OSDI paper ("Coz: Finding Code that Counts with Causal Profiling," Curtsinger & Berger) introduced *causal profiling*: instead of "this function uses 5% of CPU," ask "if I made this function 10% faster, how much would the program speed up?" Wildly more useful for distributed systems. A long-term Phase 25+ extension: causal profiling for fabric migrations.

### 8.5 Flame graphs (Brendan Gregg)

Standard tool: convert a stream of stack samples into a flame visualization. We have causal trace data which is even richer. Trivial Python script to convert. **Make this the default migration-debugging view.**

### 8.6 OpenTelemetry trace export format

Even if you don't implement it day one, **be aware that OpenTelemetry's OTLP** (OpenTelemetry Protocol) is the de-facto open standard for trace export. Your trace records map 1:1 to OTLP spans. Adding an OTLP exporter later means your kernel traces can flow into Jaeger, Tempo, Honeycomb, Datadog, etc. for visualization. Bonus feature for future-you.

### 8.7 Phi Accrual failure detector (preview of Phase 17)

Heartbeat-with-timeout failure detection is binary and brittle. Phi Accrual (Hayashibara et al., "The Phi Accrual Failure Detector") tracks heartbeat *inter-arrival times* statistically and produces a continuous "suspicion level" (phi). A peer at phi=1 is fine; phi=8 is "almost certainly dead." Applications choose their own threshold. Fabric uses this for peer health (Phase 17). The ledger surfaces `phi_suspicion_level` per peer (see 6.1).

### 8.8 Reservoir sampling for unbounded streams

Keeping "the last 1000 trace events for pid X" sounds simple but is tricky for high-volume processes. Reservoir sampling (Vitter, 1985) maintains a *uniform random sample* of size K from an unbounded stream in O(1) memory and O(1) per-insert. Apply to per-process trace retention.

---

## 9. Implementation Phasing

Suggested breakdown of Phase 12-13 into bite-sized milestones:

### Phase 12.1 (1 week) — Event record and ring buffer
- Define `kv_trace_event_t` (64 bytes)
- Per-CPU ring buffer (start with size = 64K events = 4 MB)
- Lockless `kv_trace_emit_` implementation
- Define category/type enums
- `KV_TRACE` macro
- Console dumper: `trace dump` shows last 100 events decoded

### Phase 12.2 (1 week) — Sprinkle trace points
- Add trace points at every interesting kernel site (use Document 01 Seed 1.6 as the guide)
- Include process lifecycle, scheduler, page faults, syscalls, block I/O, IRQs
- Verify no measurable perf regression (microbenchmark `getpid()` loop)

### Phase 12.3 (1 week) — Causal chaining
- Per-CPU `current_event_id` variable
- Auto-set `parent_event_id` on emit
- Per-process `trace_id` field
- Verify trace shows correct parent-child chains for a simple `echo`

### Phase 12.4 (1 week) — Variable payloads + filtering
- Payload pool
- `KV_TRACE_WITH_PAYLOAD` variant
- Filter struct and apply at emit
- `trace filter` shell command

### Phase 13.1 (1 week) — Per-process counters
- `kv_proc_ledger_t` struct
- Increment sites for every counter
- `ledger pid N` shell command

### Phase 13.2 (1 week) — Per-device + per-node aggregates
- `kv_device_ledger_t`, `kv_node_ledger_t`
- Increment sites
- `ledger node`, `ledger device N` commands

### Phase 13.3 (1 week) — HDR Histograms
- Implement `kv_hdr_histogram_t`
- Apply to block I/O latency, syscall latency, scheduler latency
- `ledger latency device nvme0` command shows percentiles

### Phase 13.4 (1 week) — Snapshot ring
- Periodic timer copies node ledger to snapshot ring
- `ledger history --window 10s` shows trend

### Phase 13.5 (1 week) — Doctor v1
- Rule framework
- Implement 4-5 starter rules
- `doctor` and `explain` shell commands

**Total: ~9 weeks for Phases 12 + 13.** Pad to 10-12 weeks for testing and polish.

---

## 10. Tooling and Commands

The shell commands that make this real:

```sh
# Trace
trace on                              # enable global tracing
trace off
trace status
trace filter --cat fabric,proc        # only these categories
trace filter --pid 0:42
trace filter --sample 100             # 1-in-100
trace dump --count 100                # last 100 events decoded
trace dump --binary > /tmp/trace.bin  # binary export
trace flame --pid 0:42                # generate flamegraph data
trace show <trace_id>                 # all events for a trace_id

# Ledger
ledger node                           # this node's aggregate metrics
ledger pid 0:42                       # per-process metrics
ledger device nvme0                   # per-device metrics
ledger latency device nvme0           # percentile histogram
ledger history --window 60s           # snapshot ring
ledger top --by cpu                   # top 10 processes by CPU
ledger top --by io_bytes
ledger top --by faults

# Doctor
doctor                                # full diagnosis
doctor rules                          # list active rules
doctor watch                          # continuous monitoring
explain pid 0:42                      # ad-hoc explain
explain block device nvme0
explain migration latest              # future (Phase 18+)
explain fabric peer node=3            # future (Phase 17+)
```

These commands are your demos. Practice running them. Screenshot them. Write blog posts about them. The doctor command alone is enough to make systems people pay attention to your project.

---

## 11. Common Pitfalls

### 11.1 Tracing the tracer

The most common bug: a trace call inside the trace emission path. Causes infinite recursion. **Rule:** every function called from `kv_trace_emit_` is annotated `__no_trace` and contains no `KV_TRACE` calls.

### 11.2 TSC reading inside locks

TSC reads are cheap but not free (~20-30 cycles). Calling RDTSC inside a hot spinlock multiplies contention. Solution: read TSC *once* per event, outside any user lock.

### 11.3 Histogram bucket explosion

A naive histogram with linear buckets covering 1ns..1hour needs 3.6 trillion buckets. HDR Histograms solve this with log-scale buckets (~600 total). Use the proven structure; don't roll your own.

### 11.4 Atomic counter contention on SMP

When you add SMP later (Phase 25+), a single global counter incremented from every CPU becomes a contention nightmare. Use per-CPU counters that get summed on read (RCU-style). Design the API now so the implementation can change later: `kv_counter_inc(c)` is the API, the underlying type can be per-CPU.

### 11.5 Drift in offline trace decoding

Decoder must understand the producer's TSC frequency, endianness, struct layout. **Include a manifest at the start of every binary trace dump** with these metadata. Without it, traces taken on different kernels are unreadable.

### 11.6 Forgetting to disable tracing in tight loops

Microbenchmarks of "syscall overhead" will be wildly distorted if tracing is on. Provide `KV_TRACE_DISABLED` block macro:

```c
KV_TRACE_DISABLED({
    /* benchmark loop */
});
```

### 11.7 Doctor rules that flap

A rule that fires for one sample and clears the next is noise. Every rule should require *sustained* evidence: "WARN if X true for 5+ consecutive samples." Hysteresis prevents alert fatigue.

---

## 12. Success Criteria

End-of-Phase-12 demo:
```sh
> trace on
> /bin/copy /a /b
> trace flame --pid 0:5 > /tmp/flame.txt
> trace show <last trace_id>
```
Output: a complete causal timeline of `copy`'s execution, with parent-child links making it obvious *why* each event happened.

End-of-Phase-13 demo:
```sh
> /bin/heavy_proc &
> /bin/io_proc &
> doctor
```
Doctor accurately identifies the heavy process, the I/O-bound process, dominant resource consumers, and queue saturation if applicable. Output is human-readable and would help a real user understand "what is wrong with my system."

After these two phases, **you have everything the fabric needs to be observable when it ships in Phase 16+**. Every migration will be traceable. Every scheduling decision will be explicable. No black boxes.

---

## Closing Note

Observability is the boring, unsexy work that determines whether your distributed kernel is a research success or a "well, I had an idea once" footnote. Every working SSI from the academic literature shipped serious tracing. Every failed one had ad-hoc logs that "worked" until they didn't.

The two phases this document covers — TraceOS and the Ledger — are ~9 weeks of work. They are also the **single highest-leverage investment** you can make before touching fabric code. Skip them, and you'll spend the next year guessing why migrations are slow. Build them, and you'll spend that year *answering* why migrations are slow — with data, with traces, with histograms.

The fabric you'll build in Phases 16-22 stands on top of this. Build the foundation right.

---

**Next document to write:** `dssi_03_checkpoint_restore.md` — Phases 14-15. The single hardest primitive in DSSI: freezing a running process and recreating it elsewhere, byte-perfect. We'll build and test it entirely locally before any networking is involved.
