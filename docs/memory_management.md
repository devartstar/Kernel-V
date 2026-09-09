# Kernel-V Physical Memory Management

## Core Principles:

1. Fast + Reliable - deterministic worst case O(1).
2. Least wastage of Pooled Resources - low internal fragmentation.
3. SSI forward - the physical bytes can be in local or remote machine.
    the allocator must have a seam where a regions backing memory
    can later be node-affinity or remote (RDMA registered). 
    Since in SSI memory is pooled across machines.

## Ideas from other pmm implementation:

┌────────────────┬───────────┬──────────────────────────────────────┬────────────────────┐
│ Family         │ Examples  │ The one idea worth stealing          │ Fatal flaw for us  │
├────────────────┼───────────┼──────────────────────────────────────┼────────────────────┤
│ Boundary-tag / │ glibc     │ Exact fit via split + coalesce       │ Non-deterministic; │
│ sequential-fit │ malloc,   │                                      │ external frag;     │
│                │ dlmalloc  │                                      │ per-object header  │
├────────────────┼───────────┼──────────────────────────────────────┼────────────────────┤
│ Buddy          │ Linux     │ O(log n) coalescing, no external     │ ~2× internal waste │
│                │ page      │ frag at page grain                   │ on objects         │
│                │ allocator │                                      │                    │
├────────────────┼───────────┼──────────────────────────────────────┼────────────────────┤
│ Slab / SLUB    │ Bonwick,  │ Size-class caches; page descriptor → │ Bounded internal   │
│                │ Linux     │ O(1) ptr→meta, zero per-object       │ waste; awkward for │
│                │           │ header                               │ odd large sizes    │
├────────────────┼───────────┼──────────────────────────────────────┼────────────────────┤
│ Per-CPU/arena  │ tcmalloc, │ Sharded caches = lock-free fast      │ Complexity         │
│                │ jemalloc  │ path, multicore scaling;             │                    │
│                │           │ quarter-spaced classes               │                    │
├────────────────┼───────────┼──────────────────────────────────────┼────────────────────┤
│ mimalloc       │ MS        │ Sharded per-page free list + atomic  │ Complexity         │
│                │           │ thread-free list → branch-light      │                    │
│                │           │ alloc, cheap remote free             │                    │
├────────────────┼───────────┼──────────────────────────────────────┼────────────────────┤
│ TLSF           │ real-time │ Worst-case O(1) alloc and free for   │ More metadata      │
│                │ systems   │ arbitrary sizes, via two-level       │                    │
│                │           │ bitmap + ffs, with coalescing        │                    │
├────────────────┼───────────┼──────────────────────────────────────┼────────────────────┤
│ Region / bump  │ arenas,   │ O(1) alloc, zero fragmentation,      │ No individual free │
│                │ obstacks  │ mass-free                            │                    │
└────────────────┴───────────┴──────────────────────────────────────┴────────────────────┘

## Proposed: KAlloc

Based on size of the memory to be allocated there will be four tiers:
1. T1 SLAB <= 2KB : Quater spaced classes, or per-cpu magazine.
2. TLSF > 2KB <= 64KB : O(1) fit, coalescing (odd sizes fit well)
3. PAGE-RUN > 64KB : page multipliers from buddy
4. T0 substrate Buddy over 16MB heap window; hands frames/runs to T1/T2/T3

• T1 — Slab, small objects (the 90% path). Quarter-spaced classes (bounded ~25% waste), page-header slab (zero per-object overhead), per-CPU magazines so the hot path is lock-free and worst-case O(1) (pop a pointer). This is where nearly all allocations land, so it's the one we build first and tune hardest.
• T2 — TLSF, the "awkward middle." Exactly the sizes where slab would waste too much and buddy rounds to 2× — a 5 KB or 40 KB buffer. TLSF gives O(1) alloc + free with tight fit and coalescing. This is the piece that resolves your original 20-byte-waste worry at the sizes where it actually costs real memory.
• T3 — Page-run, huge. Whole-page multiples straight from the buddy substrate; header cost is negligible at this scale.
• T0 — Buddy substrate. Manages the 16 MB heap window as power-of-2 page runs, O(log n) coalescing, feeds the upper tiers. No external fragmentation at page granularity.

### Commited to Core:
1. Worst Case O(1) as a hard invariant for every tier. Note. not Avergae case or
   amortized time complexity.
2. One unified page/segment descriptors. (SLUB/mimalloc style): `prt & mask -> descriptor`. 
    descriptor tell us tier, size class, and owner in O(1).
3. Node Affinity & Readiness Arenas. The SSI stream.

### Concepts specific to SSI memory management.
1. Arena (multi page region) - a tier carves memory from the arena.
2. Arena is desined from day 1 as the future unit of NUMA/node affinity and RDMA
   registration.
3. In SSI a pooled memory can span across machines. ie. pages backed by arena
   can be in remote machines. Then a local pointer bridges to another nodes RAM.
4. We forward plan for remote memory and design seam. 


### Reliability by construction (cross-cutting, cheap to add)

Red zones + poison-on-free, double-free detection via per-slab bitmaps, optional free quarantine (delayed reuse) to catch use-after-free, and per-class/per-tier counters exposed as metrics (the measure-don't-guess tenet). These ride on the unified descriptor essentially for free.

### Plus a specialized complement (not the general path)

Offer a region/bump allocator as a separate tool for phase-scoped lifetimes — boot-time init, per-RPC scratch buffers — where mass-free is O(1) and fragmentation is zero. Right tool for lifetime-bounded work; not the general allocator.

## Implementation Plan

### Reality check — innovation must not block us

The full 4-tier design is the architecture, not the first commit. Building it all at once is how a hobby kernel stalls for 3 months. Pragmatic build order:

1. Now: T1 slab (quarter-spaced, page-header) + T0 as a dead-simple page-run bump over the heap window. Stub T2/T3 by routing large requests to page-runs directly. This covers 95%+ of real allocations and unblocks A1 (wait-queues), the NIC, RPC — everything downstream.
2. Later, only if profiling shows it: add per-CPU magazines (when we're actually multicore), swap the simple T0 for a real buddy, and add TLSF for T2 (when the awkward-middle actually appears in traces).
3. Much later (Part on distributed memory): light up the arena node-affinity/RDMA seam.

So we design the interface and descriptor for all four tiers now (so nothing needs retrofitting), but implement only T1 + a trivial large path first. Deterministic, efficient, SSI-ready — without stalling.

