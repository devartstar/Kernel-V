# DSSI 06 — Live Migration & Automatic Scheduling (Phases 20-22)

> **Scope:** Phases 20, 21, and 22. Phase 19 gave us manual migration with pauses measured in seconds. This document collapses pause time to tens of milliseconds *and* removes the human from the loop. By the end of Phase 22, processes flow across the fabric on their own — when a node gets hot, the scheduler picks the right victim, picks the right target, and migrates it without anyone noticing.
>
> **Position in the master plan:** Part 6 of 8. After this, the only major mechanisms still to build are the distributed VFS (Doc 07: Phase 24) for moving file resources between nodes, RDMA/CXL for hardware acceleration (Doc 07: Phases 25-26), and security/trust for crossing trust boundaries (Doc 07: Phase 27).
>
> **What changes for the user after Doc 06:** today they type `migrate <pid> <node>`. After Doc 06, they type nothing. They run their workload. The kernel migrates it. They look at `fabric ps` and notice processes have moved. Or they don't notice, because performance is just *better*.

---

## 0. Thinking Process — Why These Three Phases, In This Order

### Why scheduler before live migration

Tempting to do it the other way: optimize the migration mechanism first, then write a policy that uses the fast mechanism. But that's backwards because:

1. **A bad policy with a fast mechanism creates more harm than a bad policy with a slow mechanism.** Slow migration is self-limiting — operators won't trigger 100 migrations in 10 seconds because each takes 2 seconds. Fast migration is *not* self-limiting. A naive auto-scheduler with sub-100ms migration could thrash a cluster into uselessness in seconds (migration storms — see §1.6 of Phase 20).
2. **The scheduler's data needs drive what the migration layer must expose.** We need to know how dirty pages get during execution to choose pre-copy vs post-copy. We need to know proxy fraction to decide "migrate process back home" vs "migrate the resource here." We don't know what telemetry matters until we have a policy consumer.
3. **A working scheduler with stop-and-copy migration is already useful.** The phase ordering means we always have a complete system at every checkpoint, not "the policy needs the optimization to even be useful."

The model here is borrowed from Borg (Verma et al., EuroSys 2015): build the scheduler first against a coarse mechanism, then iterate the mechanism to remove the bottlenecks the scheduler discovers.

### Why pre-copy before post-copy

Pre-copy ships pages *while the process keeps running*. The risk of pre-copy is **non-convergence**: a process dirties pages faster than we can ship them. When this happens, you abort or fall back to stop-and-copy. **The process is always safe**; the source has a complete state at all times.

Post-copy ships the bare minimum (registers, kernel stack), resumes on the target, and *demand-faults* pages from source as the process touches them. The risk of post-copy is **source failure during the fault window**: if source dies before all pages are pulled, the process is corrupted in unrecoverable ways. There's no fallback because there's no complete copy anywhere.

Pre-copy is *robust but possibly slow*. Post-copy is *fast but fragile*. You need both, and the right combination depends on the workload. So:

| Phase | Approach | Risk profile |
|-------|----------|--------------|
| Phase 21 | Pre-copy + checkpoint replicas | Always safe; sometimes slow |
| Phase 22 | Post-copy + hybrid pre/post | Fast when network healthy; needs Phase 21's replica safety as backstop |

Building pre-copy first means Phase 22's post-copy inherits the replica machinery for free. Trying to build post-copy first would require building replicas *and* demand-fault *and* fall-back logic simultaneously — three hard problems entangled.

### Why hybrid is its own piece, not "just an option"

Pre-copy and post-copy converge on the right answer through different paths. Their failure modes are different. Their telemetry needs are different. Their backoff behavior is different. Making "hybrid" a runtime mode-switch decision means you need machinery that can run *either* at any time, plus a controller that decides which to use. That controller, with its dirty-rate measurement, mode-switch protocol, and tunable thresholds, is non-trivial code — it deserves its own design block within Phase 22.

This three-phase split (scheduler, pre-copy, post-copy+hybrid) is also how the live-migration literature splits the work. The seminal pre-copy paper (Clark et al., NSDI 2005, "Live Migration of Virtual Machines") is exclusively about pre-copy. The seminal post-copy paper (Hines & Gopalan, VEE 2009) is exclusively post-copy. The hybrid analyses (e.g., Hu et al., Cloud Computing 2013) came later and treated combination as its own contribution.

### The deeper principle: migration is a feedback loop, not a function call

A typical kernel call is request → execute → return. Migration is *not* that. It's:

```
sense load -> decide to migrate -> pick target -> begin migration ->
   observe progress -> adapt strategy -> retry or fall back ->
     observe outcome -> update policy -> sense load again
```

Every phase in Doc 06 is part of that loop. Phase 20 is *sense* and *decide*. Phase 21 is *execute with observation*. Phase 22 is *adapt*. The Resource Ledger and TraceOS from Docs 02-04 are the eyes the loop runs on.

---

## 1. Phase 20 — Distributed Scheduler

### 1.1 Goals

1. The scheduler runs on every node, every ~500ms.
2. On each tick, the scheduler decides:
   - Is this node overloaded? If yes, pick a victim process to migrate away.
   - Is this node underloaded? If yes, advertise spare capacity (already happens via gossip).
3. When migrating away, the scheduler picks a target using cost/benefit analysis.
4. The scheduler is **anti-thrash**: a process that just migrated isn't migrated again for at least N seconds.
5. The scheduler is **observable**: every decision (including "no decision") emits a trace event explaining why.
6. The scheduler is **pluggable**: the policy is a discrete function that can be swapped without changing the mechanism. Default policy = "round-robin load balancing with home-affinity preservation."

What's explicitly NOT in Phase 20:
- Cluster-wide global scheduler / leader election. (Phase 20 is per-node + gossip; that's it.)
- Affinity groups, gang scheduling, deadline scheduling. (Stretch goals for Phase 23+.)
- Migration of system processes (init, fabric supervisor, deputy threads). (Hard-pinned.)
- Migration policies that read from userspace. (Future hook; not now.)

### 1.2 The Per-Node Scheduler Tick

```c
void scheduler_tick(void) {
    kv_node_state_t my_state = collect_my_state();

    // Phase 1: am I overloaded?
    if (my_state.cpu_util_pct < KV_SCHED_HOT_THRESHOLD) {
        KV_TRACE_EMIT(SCHED_TICK_IDLE, my_state.cpu_util_pct);
        return;   // nothing to do
    }

    // Phase 2: find candidate processes to evict
    struct kv_proc *victim = pick_victim_process();
    if (!victim) {
        KV_TRACE_EMIT(SCHED_NO_VICTIM, my_state.cpu_util_pct);
        return;
    }

    // Phase 3: find candidate target nodes
    kv_fabric_peer_t *target = pick_target_node(victim);
    if (!target) {
        KV_TRACE_EMIT(SCHED_NO_TARGET, victim->pid.local_id);
        return;
    }

    // Phase 4: anti-thrash check
    if (!can_migrate_now(victim, target)) {
        KV_TRACE_EMIT(SCHED_THROTTLED, victim->pid.local_id, target->node_id_64);
        return;
    }

    // Phase 5: commit
    KV_TRACE_EMIT(SCHED_MIGRATE_DECISION, victim->pid.local_id, target->node_id_64);
    migrate_async(victim, target);   // non-blocking; uses Phase 18-22 machinery
}
```

This runs every 500ms. The cost when idle: one pass over local proc list (O(n)), one comparison of CPU%, one trace event. Negligible.

### 1.3 Victim Selection — The Cost Side

The cost of migrating process P is *not* its size. It's the expected disruption: how long is P frozen, how many syscalls will be proxied afterward, how likely is P to come back home, etc.

We compute a **migrability score**:

```c
typedef struct kv_migrability {
    uint32_t base_score;             // higher = better candidate

    // Negative factors (subtract)
    uint32_t recent_migration_penalty;  // migrated in last 30s = -1000
    uint32_t fd_count_penalty;          // each open FD = -10 (proxy overhead later)
    uint32_t home_locality_penalty;     // not currently on home = -50
    uint32_t kernel_thread_penalty;     // kthread = INFINITE (cannot migrate)
    uint32_t pinned_penalty;            // operator-pinned = INFINITE

    // Positive factors (add)
    uint32_t cpu_intensity_bonus;       // high CPU% = good migration target
    uint32_t memory_size_bonus;         // medium memory = ideal (1-100MB)
    uint32_t age_bonus;                 // long-running = more stable workload
} kv_migrability_t;

int compute_migrability(struct kv_proc *p) {
    int score = 100;   // baseline

    if (p->is_kernel_thread || p->migration_pinned) return -1;   // ineligible

    if (now_ms() - p->last_migration_ms < 30000) score -= 1000;

    score -= p->fd_table_count * 10;
    if (p->exec_node_id != p->home_node_id) score -= 50;  // already away

    score += (p->cpu_util_pct - 50) * 2;  // bonus above 50% CPU
    if (p->resident_kb >= 1024 && p->resident_kb <= 102400) score += 30;

    uint64_t age_s = (now_ms() - p->start_ms) / 1000;
    if (age_s > 60) score += min(age_s / 60, 30);  // cap age bonus at 30

    return score;
}
```

Pick the process with the highest score. If nothing scores above 0, no migration this tick.

This is a deliberately simple linear scoring function — it's tunable, transparent, and works. Phase 23+ may replace it with a learned cost model, but YAGNI for now.

### 1.4 Target Selection — The Benefit Side

We have peer state from gossip (Doc 04). Filter and rank:

```c
kv_fabric_peer_t *pick_target_node(struct kv_proc *victim) {
    kv_fabric_peer_t *best = NULL;
    int best_score = -1;

    for_each_peer(p) {
        // Hard filters
        if (p->state != KV_FPS_ALIVE) continue;
        if (p->phi.last_score > 2.0) continue;
        if (p->resources.mem_free_kb < victim->resident_kb * 2) continue;
        if (p->resources.cpu_util_pct > KV_SCHED_TARGET_HOT) continue;

        // Score: prefer low CPU, plenty of memory, low phi (= reliable network)
        int score = 100;
        score -= p->resources.cpu_util_pct;            // lower CPU = better
        score += min(p->resources.mem_free_kb / 1024, 100);  // more mem = better, capped
        score -= (int)(p->phi.last_score * 10);        // lower phi = better

        // Home affinity bonus: if victim's home is this peer, big bonus
        if (p->node_id_64 == victim->home_node_id_64) score += 50;

        if (score > best_score) {
            best = p;
            best_score = score;
        }
    }

    return best;
}
```

The **home-affinity bonus** is critical. A process whose home is node-a but currently runs on node-b is a strong candidate to migrate back to node-a if node-a has capacity. This naturally reduces syscall proxy traffic over time without explicit coordination.

### 1.5 Anti-Thrash & Hysteresis

A scheduler that fires on every tick will oscillate. We need three forms of damping:

1. **Per-process cooldown:** a process that just migrated is ineligible for 30 seconds. Already baked into the migrability score (`recent_migration_penalty`).
2. **Per-node migration budget:** a node cannot initiate more than K migrations per minute (default K=5). Prevents migration storms when a node is genuinely overloaded.
3. **Hysteresis on triggering:** the scheduler starts migrating at CPU 80% but only stops at CPU 60%. This 20-point dead band prevents flapping when load hovers around the threshold. Same approach as TCP slow-start / fast-recovery.

These three controls compose: even if many processes look migratable, the budget caps the rate; even if one process is migrating constantly, the cooldown stops it; even if load oscillates, hysteresis ignores the noise.

### 1.6 Migration Storm Prevention

The classic distributed-scheduler failure: node A is overloaded → it migrates 50 processes to node B in 1 second → now node B is overloaded → it migrates them all to node C → ringing.

Three defenses:

1. **Each migration consumes budget on the *target* too.** A node only accepts K migrations per minute. Once full, it rejects with `MIGRATE_REJECT_BUDGET`.
2. **Migration decisions use gossiped state that's up to 200ms old.** If A and B both decide to migrate to C in the same tick, only one wins; the other gets `MIGRATE_REJECT_BUSY` and waits.
3. **Backoff after rejection.** A scheduler that gets MIGRATE_REJECT delays its next target-pick attempt by 2× the gossip interval. Prevents synchronized retry storms.

These are direct lessons from the Akka cluster postmortems and the Kubernetes scheduler's pod-binding contention literature.

### 1.7 The Policy Is Pluggable

The actual decision logic lives behind a single function pointer:

```c
typedef struct kv_scheduler_policy {
    const char *name;
    void (*tick)(void);
    struct kv_proc *(*pick_victim)(void);
    kv_fabric_peer_t *(*pick_target)(struct kv_proc *);
    bool (*should_accept)(kv_gid_t source, struct kv_migrate_request *);
} kv_scheduler_policy_t;

extern kv_scheduler_policy_t kv_sched_default;     // §1.3-1.6 above
extern kv_scheduler_policy_t kv_sched_manual;      // never migrates automatically
extern kv_scheduler_policy_t kv_sched_chaos;       // for testing; random migrations
```

Set at boot via `fabric.cfg`: `scheduler_policy=default`. Operators can switch policies online via `fabric scheduler set <name>`.

Future policies (Phase 23+) will plug in here: energy-aware, latency-sensitive, affinity-group-aware, deadline-driven, ML-guided.

### 1.8 Shell Surface

```
fabric scheduler status              # which policy, budget remaining, last tick
fabric scheduler decisions           # last 50 decisions with reasons
fabric scheduler tick                # force a tick now (debugging)
fabric scheduler set <policy>        # change policy
fabric scheduler pin <pid>           # exclude pid from migration
fabric scheduler unpin <pid>
```

Every command emits trace events. `fabric scheduler decisions` is the most-used in practice — it lets you see exactly why the scheduler did (or didn't) act.

### 1.9 Phase 20 Implementation Phasing (4 weeks)

| Week | Deliverable | Verifiable result |
|------|-------------|-------------------|
| 1 | Scheduler skeleton, tick loop, KV_TRACE events for all decision outcomes | `scheduler tick` runs; emits SCHED_TICK_IDLE |
| 2 | Victim selection, migrability scoring, pinning, kernel-thread exclusion | `scheduler pick-victim` returns sensible choices |
| 3 | Target selection, gossiped-state filtering, home-affinity bonus | 2-node demo: spin up CPU work, watch automatic migration |
| 4 | Anti-thrash, storm prevention, policy plug-in plumbing, shell surface | 4-node chaos test: no migration storms, eventual load balance |

---

## 2. Phase 21 — Pre-Copy Migration

### 2.1 Goals

1. Migration pause time drops from "seconds" to "<100ms" for typical processes.
2. The pre-copy mechanism iteratively ships dirty pages while the process keeps running on source.
3. Convergence detection: if dirty rate stays above ship rate, abort to stop-and-copy.
4. Compression in the page-shipping path (LZ4 in hot path, ZSTD optional for cold pages).
5. **Checkpoint replicas**: while shipping to primary target, also ship to a secondary peer. Lifts the "home node death loses process" limitation from Phase 18.
6. Bandwidth throttling: pre-copy uses no more than 50% of the fabric link's measured capacity, leaving headroom for gossip and proxy traffic.

### 2.2 The Pre-Copy Algorithm

```
SOURCE                                  TARGET                  REPLICA
  |                                       |                       |
  | 1. negotiate; allocate frozen shell on target & replica
  | 2. snapshot VMA layout; build initial "dirty" set = ALL anon pages
  |
  +-------- iteration N (N=1,2,3,...) ---------------+
  |                                       |          |
  |  3. mark page-table read-only on dirty pages    |
  |     (so we can detect re-dirty)                 |
  |  4. compress + stream the dirty page set
  |     -----------------MIGRATE_CHUNK------------->|
  |                                       |          |
  |                                       |   (also)-----> REPLICA
  |                                       |          |
  |  5. as process runs, hardware sets PTE.dirty   |
  |     bits on touched pages -> new dirty set     |
  |
  |  6. measure: |dirty_set_N+1| vs |dirty_set_N|
  |     if shrinking AND below threshold (e.g. <100 pages):
  |       proceed to STOP
  |     else if dirty rate > ship rate after 3 iterations:
  |       ABORT pre-copy; fall back to stop-and-copy
  |     else:
  |       go to iteration N+1
  +---------------------------------------------------+
  |
  | 7. STOP: freeze process (Phase 18 freeze)
  | 8. ship final dirty set + non-page state (registers, FDs as Phase 19)
  | 9. CRC + RESTORED
  | 10. atomic switchover (Phase 18 §2.4)
  | 11. thaw on target
  | 12. tombstone source; retain replica for safety window
```

The **target pause time** is just steps 7-11, typically 10-50ms for converged workloads.

### 2.3 Dirty Page Detection

Two implementations, picked by hardware feature detection at boot:

**Option A: PTE dirty bits (universal).**
Mark dirty pages read-only at iteration start. When the process writes, it page-faults; we mark the page as dirty for the next iteration and restore write permission. Cost: one page fault per touched page per iteration. Acceptable up to ~10,000 dirty pages per iteration.

**Option B: Intel PML (Page Modification Logging).**
Available on Intel since Broadwell (2014). The CPU writes a log buffer of dirty page numbers automatically; the kernel just reads the buffer. No page faults needed. Scales to ~1M dirty pages per second per CPU.

We detect PML at boot via CPUID; if present, use Option B. Else Option A. The two backends present the same `dirty_set` interface to the rest of pre-copy.

This is exactly how QEMU's live migration evolved: PTE-tracking until 2015, then PML when supported.

### 2.4 Convergence Logic

The classic pre-copy failure: a process dirties memory faster than the network can ship pages. We must detect this and bail to stop-and-copy.

```c
typedef struct kv_precopy_state {
    uint32_t iteration;
    uint64_t dirty_set_size[8];   // last 8 iteration sizes
    uint64_t iteration_time_ms[8];
    uint64_t bytes_shipped_total;
    uint64_t convergence_threshold;   // default 100 pages = 400KB
} kv_precopy_state_t;

enum kv_precopy_verdict {
    PRECOPY_CONTINUE,
    PRECOPY_STOP,        // converged; brief pause to ship rest
    PRECOPY_ABORT,       // not converging; fall back to stop-and-copy
};

enum kv_precopy_verdict precopy_verdict(kv_precopy_state_t *s) {
    if (s->iteration < 2) return PRECOPY_CONTINUE;

    uint64_t cur = s->dirty_set_size[s->iteration % 8];
    uint64_t prev = s->dirty_set_size[(s->iteration - 1) % 8];

    // Converged: dirty set small enough to ship in one final stop-and-copy burst
    if (cur < s->convergence_threshold) return PRECOPY_STOP;

    // Not converging after 5 iterations: bail
    if (s->iteration >= 5 && cur >= prev * 0.9) return PRECOPY_ABORT;

    // Stop-and-copy is better if dirty rate is so high pre-copy doesn't help
    if (s->iteration >= 3 && cur > 10000) return PRECOPY_ABORT;

    return PRECOPY_CONTINUE;
}
```

When verdict is ABORT, log it (`KV_TRACE_PRECOPY_ABORT`), freeze the process, and fall through to Phase 18's stop-and-copy path. The source state is intact; nothing is lost.

### 2.5 Compression in the Hot Path

Pages compress beautifully — most contain lots of zeros (uninitialized memory, page tables, sparse data structures). LZ4 typically achieves 2-3× on real workloads at 500MB/s per core.

```c
struct kv_compressed_chunk {
    uint32_t orig_size;        // before compression
    uint32_t comp_size;        // after compression
    uint8_t  algorithm;        // 0=none, 1=lz4, 2=zstd
    uint8_t  data[];
};
```

Algorithm choice:
- LZ4 for inline iterative pages (latency matters)
- ZSTD level 3 for the final stop-and-copy burst (size matters more than latency for the freeze duration)
- None if profiling shows compression CPU > network savings (e.g., 100Gbps fabric)

Decompression on target is single-threaded per stream but multi-stream-parallel across migrations.

### 2.6 Bandwidth Throttling

The fabric link carries gossip, proxy syscalls, and migration traffic. We must not let migration starve the others.

```c
typedef struct kv_link_bw_governor {
    uint64_t measured_capacity_Bps;   // updated by EWMA from recent transfers
    uint64_t migration_quota_pct;     // default 50
    uint64_t current_migration_bps;
    spinlock_t lock;
} kv_link_bw_governor_t;

void migration_pace_send(size_t bytes) {
    uint64_t quota = governor.measured_capacity_Bps * governor.migration_quota_pct / 100;
    if (governor.current_migration_bps >= quota) {
        // sleep just long enough to come under quota
        uint64_t excess = governor.current_migration_bps - quota;
        kv_msleep(excess / 1000);
    }
    actually_send(bytes);
}
```

Capacity is measured by occasional 1-second probes; we send a known amount of throwaway data and time it. Once a minute is plenty.

50% is the default because it leaves headroom for the *unrelated* traffic (gossip, proxy) while still allowing migration to finish in reasonable time. Operators can tune via `fabric link bw-quota`.

### 2.7 Checkpoint Replicas (Lifts the Phase 18 Limitation)

While pre-copying to target, also stream to a **replica peer** (a third node, picked by the scheduler from low-load nodes). The replica holds an immutable, complete checkpoint image of the process at the moment we begin pre-copy.

If target dies during migration → fall back: instruct replica to take over as the new target.
If source dies during pre-copy → replica still has the pre-precopy checkpoint; we lose work-in-flight but not the process.
If both source and replica die simultaneously → process is lost. Unavoidable with 2-of-3 durability.

For higher durability, operators can configure N>1 replicas. Cost scales linearly. Default N=1 (one replica) covers the most-common single-fault case.

The replica image is *garbage-collected* a few seconds after successful migration (default 10s). This gives the scheduler time to confirm the migration was good, while bounding storage cost.

### 2.8 Phase 21 Test Strategy

1. **Pause-time measurement:** 100MB process with low write rate; measure pause time. Target <50ms.
2. **High-dirty-rate convergence:** process writes 10MB/s into anon memory; verify pre-copy converges or aborts cleanly to stop-and-copy.
3. **Pathological dirty rate:** process writes 1GB/s; verify pre-copy aborts after ≤5 iterations, falls back to stop-and-copy successfully.
4. **Compression ratio test:** verify LZ4 path achieves >1.5× on realistic memory contents.
5. **Bandwidth throttling test:** start a 1GB migration on a 100Mbps link; verify gossip latency stays under 500ms throughout.
6. **Replica failover test:** during pre-copy, kill the primary target; verify migration succeeds via replica.
7. **Source failure test:** kill source after pre-copy starts; verify process is recoverable from replica.
8. **PML vs PTE-bits test:** run on both Intel (PML) and AMD/older Intel (PTE bits); verify identical correctness, measure perf delta.

### 2.9 Phase 21 Implementation Phasing (6 weeks)

| Week | Deliverable | Verifiable result |
|------|-------------|-------------------|
| 1 | Dirty-page-tracking backend (PTE bits version); iteration loop skeleton | Iteration count visible in trace; one full pass works |
| 2 | LZ4 integration; compressed chunk format | Compression ratio metric appears in traces |
| 3 | Convergence detector; abort-to-stop-and-copy fallback path | Pathological-dirty test correctly aborts |
| 4 | PML backend; runtime detection | Identical correctness on both backends |
| 5 | Replica streaming; failover protocol | Replica failover test passes |
| 6 | Bandwidth governor; integration tests; perf tuning | All 8 tests pass; pause-time targets met |

---

## 3. Phase 22 — Post-Copy & Hybrid Migration

### 3.1 Goals

1. **Post-copy mode:** brief pause (<20ms) ships registers + kernel stack + page-table layout; process resumes on target immediately; pages demand-faulted from source as touched.
2. **Hybrid mode:** combine pre-copy (ship most pages while running) with post-copy (resume target before all pages are there; pull cold pages on demand). Best of both worlds.
3. **Mode selection** is automatic based on workload characteristics — operator doesn't choose.
4. KSM-style **page deduplication** opportunistically: pages with identical content (zero pages, common library text) are shipped once, referenced N times.

### 3.2 Post-Copy Mechanism

```
SOURCE                                       TARGET
  |                                            |
  | 1. brief freeze (<20ms)                    |
  | 2. ship registers, kernel stack, VMA layout, FD state
  |    -------------MIGRATE_POSTCOPY_START----->|
  |                                            |
  | 3. mark all process pages as "remote"      |
  |    on source: don't free yet, may be pulled
  | 4. notify scheduler: process is "in-flight"
  | 5. resume normal operation (don't unfreeze; this side is done)
  |                                            |
  |                                            | 6. resume process on target
  |                                            |    with pages marked NOT_PRESENT in pagetables
  |                                            |
  |                                            | 7. process touches page X
  |                                            |    -> page fault
  |                                            |    -> handler: page is "remote"
  |                                            |    -> issue MIGRATE_PAGE_REQUEST(X)
  |<---- MIGRATE_PAGE_REQUEST(X) ---------------|
  |                                            |
  | 8. read page X from local memory
  | 9. send -------MIGRATE_PAGE_RESPONSE(X)--->|
  |                                            |
  |                                            | 10. install page in pagetable
  |                                            | 11. resume the faulting instruction
  |                                            |
  |                                            | 12. eventually, all pages pulled
  |                                            |     -> notify SOURCE
  |<---- MIGRATE_POSTCOPY_COMPLETE -------------|
  |                                            |
  | 13. free local page copies; tombstone proc
```

The fragility is at step 13: if source dies between step 6 and step 12, *and* the process hasn't pulled all its pages yet, those unpulled pages are lost. Process state is corrupted.

Defenses:
- **Pre-pulling**: target proactively pulls cold pages in the background even before the process touches them. Reduces the window of vulnerability.
- **Replica fallback**: same machinery as Phase 21. If source dies, the replica can serve page requests (until *it* is fully pulled).
- **Source pause**: don't free source pages for N seconds after MIGRATE_POSTCOPY_COMPLETE. Cheap; bounds the corruption window.

### 3.3 Hybrid Mode — The Best of Both

Pre-copy first, then switch to post-copy when it's no longer worth pre-copying.

```c
void hybrid_migrate(struct kv_proc *p, kv_fabric_peer_t *target) {
    // Phase 1: pre-copy iterations
    precopy_state_t pcs = { 0 };
    while (true) {
        iteration_send_dirty(p, target);
        switch (precopy_verdict(&pcs)) {
            case PRECOPY_CONTINUE: continue;
            case PRECOPY_STOP:
                // converged; do final stop-and-copy as Phase 21
                stop_and_copy_finish(p, target);
                return;
            case PRECOPY_ABORT:
                // not converging; switch to post-copy instead of stop-and-copy fallback
                if (network_healthy() && target_alive(target)) {
                    goto post_copy_switch;
                } else {
                    stop_and_copy_finish(p, target);  // safer fallback
                    return;
                }
        }
    }
post_copy_switch:
    // Phase 2: post-copy
    post_copy_begin(p, target);   // §3.2 from step 1
    return;
}
```

The switch decision in `precopy_verdict + post_copy_switch` is:
- **Continue pre-copy** while it converges.
- **Switch to post-copy** if pre-copy doesn't converge BUT the network is healthy enough that demand-paging will be fast.
- **Fall back to stop-and-copy** as a final safety net.

This is the strategy described in the Hu et al. 2013 paper and used by recent QEMU versions.

### 3.4 Page Deduplication (KSM-Style)

Many pages have identical content. The most common: the all-zero page. Less common but still frequent: read-only code pages from shared libraries.

Maintain a **content-hash table** of pages we've shipped during migration. Before sending a page, hash it (xxhash, ~10GB/s per core). If the hash matches a previously-sent page, send a back-reference instead of the content.

```c
struct kv_page_dedup_entry {
    uint64_t content_hash;    // xxhash64
    uint64_t first_send_offset_in_stream;
};

// Per-migration table; size capped at 4096 entries (~64KB), LRU evict
struct kv_page_dedup_table {
    struct kv_page_dedup_entry entries[4096];
    int next_evict;
};

void migrate_send_page(struct kv_page_dedup_table *t, page_t page) {
    uint64_t hash = xxhash64(page.data, PAGE_SIZE);
    int idx = lookup(t, hash);
    if (idx >= 0) {
        send_dedup_reference(idx);   // 8 bytes instead of 4096
        return;
    }
    insert(t, hash);
    send_page_compressed(page);
}
```

Real-world dedup ratios for typical processes: 20-40% reduction beyond LZ4. The all-zero page alone often accounts for 10-20%.

### 3.5 Mode Selection — Letting the Kernel Choose

The scheduler from Phase 20 picks the migration mode at migration-start time:

```c
enum kv_migrate_mode pick_mode(struct kv_proc *p, kv_fabric_peer_t *target) {
    uint64_t recent_dirty_rate = p->stats.pages_dirtied_per_sec_ewma;
    uint64_t avail_bandwidth = bw_governor.measured_capacity_Bps;

    if (recent_dirty_rate * PAGE_SIZE < avail_bandwidth / 4) {
        return KV_MIGRATE_STOP_AND_COPY;   // low-dirty; simplest is best
    }

    if (recent_dirty_rate * PAGE_SIZE < avail_bandwidth) {
        return KV_MIGRATE_PRECOPY;         // pre-copy will converge
    }

    if (target_phi(target) < 0.5 && link_rtt(target) < 1000) {
        return KV_MIGRATE_HYBRID;          // network is good; demand-paging will be fast
    }

    return KV_MIGRATE_STOP_AND_COPY;       // network iffy; safer to ship it all
}
```

This is *the* policy decision that determines whether migration is fast or slow. It's explicit, observable, and tunable.

### 3.6 The "Migration Cancel" Path

What if mid-migration, the source decides this was a bad idea? (Maybe load dropped; maybe the target started looking worse.) We need a clean cancel:

- **During pre-copy:** trivial. Just stop sending. The process never paused. Target's partial state is discarded.
- **During post-copy:** harder. The process is already running on target with some pages already faulted in. We'd have to migrate it back to source. For simplicity, **post-copy is not cancellable**; once started, we commit.
- **During hybrid:** can cancel anytime up to the post-copy switch. After that, commit.

The cancellability difference is a real cost of post-copy that's worth being honest about. It makes pre-copy preferable when there's any doubt.

### 3.7 Phase 22 Test Strategy

1. **Post-copy correctness:** migrate a process with 100MB heap via post-copy; verify all data intact after all pages pulled.
2. **Pre-pull effectiveness:** measure how often the process is the first to touch a page vs the pre-puller; aim for >50% pre-pulled.
3. **Hybrid mode pause:** measure stop-the-world pause for typical workloads. Target <30ms.
4. **Mode-selection test:** for a high-dirty workload, verify scheduler picks hybrid. For low-dirty, verify it picks stop-and-copy.
5. **Source death during post-copy:** kill source after MIGRATE_POSTCOPY_START; verify replica serves the page requests successfully.
6. **Dedup ratio test:** migrate a large process with sparse memory; verify dedup achieves >20% size reduction.
7. **Network degradation mid-migration:** induce 50% packet loss during post-copy; verify graceful slowdown, no corruption.
8. **End-to-end performance suite:** run all four migration modes on standard workloads; produce a comparison table.

### 3.8 Phase 22 Implementation Phasing (7 weeks)

| Week | Deliverable | Verifiable result |
|------|-------------|-------------------|
| 1 | Post-copy mechanism: page-fault handler, remote-page-not-present marker | Post-copy can migrate a tiny process |
| 2 | MIGRATE_PAGE_REQUEST/RESPONSE wire protocol; basic demand-paging | Post-copy works for 10MB process |
| 3 | Pre-pulling background worker | Reduction in synchronous fault count visible in traces |
| 4 | Hybrid mode controller; pre-copy ↔ post-copy switching | Hybrid mode demo for high-dirty workload |
| 5 | Page deduplication; content-hash table | Dedup ratio test passes |
| 6 | Mode-selection policy; integration with scheduler from Phase 20 | Automatic mode selection works without operator |
| 7 | Source-death survivability; full perf suite; comparison table | All 8 tests pass; full perf comparison published |

---

## 4. `KV_TRACE` Events Added in This Document

```c
// Category: SCHED (0x70)
#define KV_TRACE_SCHED_TICK                 0x7001
#define KV_TRACE_SCHED_TICK_IDLE            0x7002
#define KV_TRACE_SCHED_MIGRATE_DECISION     0x7003
#define KV_TRACE_SCHED_NO_VICTIM            0x7004
#define KV_TRACE_SCHED_NO_TARGET            0x7005
#define KV_TRACE_SCHED_THROTTLED            0x7006
#define KV_TRACE_SCHED_BUDGET_EXHAUSTED     0x7007
#define KV_TRACE_SCHED_POLICY_CHANGED       0x7010

// Category: PRECOPY (0x80)
#define KV_TRACE_PRECOPY_START              0x8001
#define KV_TRACE_PRECOPY_ITERATION          0x8002
#define KV_TRACE_PRECOPY_DIRTY_RATE         0x8003
#define KV_TRACE_PRECOPY_ABORT              0x8004
#define KV_TRACE_PRECOPY_CONVERGE           0x8005
#define KV_TRACE_PRECOPY_REPLICA_OK         0x8010
#define KV_TRACE_PRECOPY_REPLICA_FAIL       0x8011

// Category: POSTCOPY (0x90)
#define KV_TRACE_POSTCOPY_START             0x9001
#define KV_TRACE_POSTCOPY_PAGE_REQUEST      0x9002
#define KV_TRACE_POSTCOPY_PAGE_DELIVERED    0x9003
#define KV_TRACE_POSTCOPY_PRE_PULL          0x9004
#define KV_TRACE_POSTCOPY_COMPLETE          0x9005
#define KV_TRACE_HYBRID_MODE_SWITCH         0x9010
#define KV_TRACE_DEDUP_HIT                  0x9020
```

The Kernel Doctor learns to explain migration mode selection, pre-copy convergence, and post-copy page-fault patterns using these events.

---

## 5. How These Phases Set Up Doc 07 (Advanced Topics)

At the end of Phase 22, migration is *production-quality* in the small. Doc 07's remaining phases address what's left:

| Doc 07 phase | Builds on what from Doc 06 |
|--------------|----------------------------|
| Phase 23: advanced scheduler (affinity, deadlines, energy) | Phase 20 policy plug-in interface |
| Phase 24: distributed VFS | Phase 21 replica machinery (also a "shared state" pattern) |
| Phase 25: RDMA / RoCEv2 transport | Phase 21-22 page-copy paths become zero-copy |
| Phase 26: CXL memory disaggregation | Hybrid migration's demand-paging extends to memory pools |
| Phase 27: security & trust | Migration becomes cross-organization-safe |
| Phase 28: global mesh / WAN | Bandwidth governor + hybrid mode tuned for high-latency links |

Every Doc 07 phase is "a refinement of, or new policy on top of, what Doc 06 built."

---

## 6. Risks & Mitigations

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Scheduler causes migration storms during partial outages | High | Per-node budget + backoff + gossip-staleness tolerance (§1.5-1.6) |
| Pre-copy non-convergence on memory-write-heavy workloads | High | Convergence detector + abort to stop-and-copy (§2.4) |
| Post-copy corruption on source death mid-faulting | High | Replicas + delayed source-page free + recommend pre-copy when in doubt |
| Compression CPU saturates kernel | Medium | Bounded threads; skip compression on >40Gbps links |
| Dirty-page tracking via PTE bits adds per-fault latency | Medium | Use PML when available; PTE path is fallback only |
| Replica becomes overloaded by being everyone's replica | Medium | Scheduler considers "is replica" load when picking new replicas |
| Bandwidth governor mis-measures, over- or under-throttles | Low | EWMA + occasional explicit probes; observable via shell |
| Dedup hash collision corrupts a page | Vanishing | xxhash64 collision space; full content compare on apparent match optional |
| Hybrid mode-switch races with target dying | Low | Mode-switch is itself atomic; replica safety net catches the rest |

---

## 7. Aggregate Timeline & Position in the Roadmap

| Phase | Weeks | Cumulative weeks (this doc) |
|-------|-------|-----------------------------|
| Phase 20: Distributed Scheduler | 4 | 4 |
| Phase 21: Pre-Copy Migration | 6 | 10 |
| Phase 22: Post-Copy & Hybrid | 7 | 17 |

Combined with prior docs:
- Phases 6-11 (Doc 01): foundation
- Phases 12-13 (Doc 02): observability — 9 weeks
- Phases 14-15 (Doc 03): slab + checkpoint — 13 weeks
- Phases 16-17 (Doc 04): fabric control plane — 9 weeks
- Phases 18-19 (Doc 05): first remote exec — 11 weeks
- **Phases 20-22 (Doc 06): live migration & auto-scheduling — 17 weeks** ← we are here

Total weeks committed across Docs 02-06: 59 weeks ≈ 14 months. The user's 1-2 year window aligns: Docs 00-06 deliver a working, automatic, live-migrating DSSI within ~15 months. Doc 07 fills out the rest of the 24-month window.

---

## 8. The Five Most Important Decisions in This Document

1. **Scheduler before optimization.** A working policy with slow migration is healthier than no policy with fast migration. Build the feedback loop before tightening it.
2. **Pre-copy is always the safe path.** Source has complete state until handoff. Post-copy can lose work if source dies — accept this tradeoff explicitly.
3. **Convergence detection is non-negotiable.** Pre-copy without an abort path will eventually corrupt a workload that dirties faster than the network. The verdict function is small but load-bearing.
4. **Replicas turn 2-node migration into 3-node durability for free.** The cost is one extra stream during migration; the benefit is surviving single-node failure.
5. **Mode selection is the kernel's job, not the operator's.** Picking stop-and-copy vs pre-copy vs hybrid is a function of measured dirty rate + measured bandwidth + measured target health. Don't ask humans.

---

## 9. What's Next

**Next document to write:** `dssi_07_advanced.md` — Phases 23-28. Six remaining phases that take Kernel-V from a working DSSI to a serious distributed operating system: advanced scheduling (affinity groups, deadlines, energy-aware), distributed VFS (move file resources between nodes; no more home-tethering for large workloads), RDMA / RoCEv2 transport (zero-copy migration at 100Gbps), CXL memory disaggregation (memory pools across the fabric), security & trust (capability-based cross-node trust, encrypted fabric), and finally the global mesh / WAN extension that lets the fabric span data centers and eventually the public internet.

Doc 07 is where Kernel-V's vision becomes a research platform that pushes past existing distributed OS work, not just rebuilds it.
