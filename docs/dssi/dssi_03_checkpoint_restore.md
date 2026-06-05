# Kernel-V DSSI — Document 03: Checkpoint & Restore (Phases 14-15)

> **The single hardest primitive in DSSI: freezing a running process and recreating it elsewhere, byte-perfect.**
> Built and tested entirely locally before any networking is involved.

This document covers two phases:
- **Phase 14**: Kernel heap and slab allocator (the prerequisite)
- **Phase 15**: Process checkpoint and restore — the core DSSI primitive

If you get checkpoint/restore working as a *local* operation (snapshot → kill → resume), then **adding the network is a transport problem, not a correctness problem**. Every later phase (stop-and-copy migration, pre-copy, post-copy, live migration) is essentially "checkpoint/restore + network."

Get this phase right and the rest of DSSI is engineering. Get it wrong and every later phase carries the debt.

---

## Table of Contents

1. [Why These Two Phases Together](#1-why-these-two-phases-together)
2. [Phase 14 — Kernel Heap & Slab Allocator](#2-phase-14--kernel-heap--slab-allocator)
3. [Phase 15 Overview — What Checkpoint Means](#3-phase-15-overview--what-checkpoint-means)
4. [Prior Art: CRIU and What It Teaches Us](#4-prior-art-criu-and-what-it-teaches-us)
5. [The Checkpoint Image Format](#5-the-checkpoint-image-format)
6. [Freezing a Process Safely](#6-freezing-a-process-safely)
7. [What to Serialize (the Inventory)](#7-what-to-serialize-the-inventory)
8. [Walking the Address Space](#8-walking-the-address-space)
9. [Restore: Reversing the Process](#9-restore-reversing-the-process)
10. [Compression](#10-compression)
11. [Testing Strategy](#11-testing-strategy)
12. [Known Limitations & Scope](#12-known-limitations--scope)
13. [Implementation Phasing](#13-implementation-phasing)
14. [Success Criteria](#14-success-criteria)

---

## 1. Why These Two Phases Together

These phases are tightly coupled:

- **Checkpoint** needs to *allocate* large variable-sized buffers (image staging, compressed output, restored objects). Your current frame-only allocator is too coarse — a 17-byte object pinning a whole 4KB page is wasteful and confusing.
- **Restore** needs to *reconstruct* kernel objects (proc, file, pipe, vma) via typed allocations. Slab caches are the natural home for these.
- The slab allocator's **per-type object pools** map 1:1 to serialization layout: every `proc_t` looks the same on disk because it lives in `proc_cache`.

So: build the heap first (Phase 14), then build C/R on top (Phase 15).

---

## 2. Phase 14 — Kernel Heap & Slab Allocator

### 2.1 The two-layer memory model

Most production kernels have a three-tier memory hierarchy:

```
┌─────────────────────────────────────────────────────┐
│  Tier 3: kmalloc / kfree (variable-size, ad-hoc)    │   ← what callers usually want
├─────────────────────────────────────────────────────┤
│  Tier 2: Slab allocator (fixed-size object caches)  │   ← per-type pools
├─────────────────────────────────────────────────────┤
│  Tier 1: Page/frame allocator (already exists)      │   ← 4KB pages
└─────────────────────────────────────────────────────┘
```

You already have Tier 1 (PMM). Phase 14 builds Tier 2 and Tier 3.

### 2.2 Slab allocator design (SLUB-style)

The original Solaris SLAB allocator was clever but complex. Linux's **SLUB** simplified it and is now the default. Recommended design.

**The model:**
- A **cache** is dedicated to one object type and one size (e.g., "proc cache" holds `proc_t` of 512 bytes each).
- A cache owns several **slabs**. A slab is one or more contiguous physical pages.
- A slab is sliced into N objects + small metadata (a freelist of unused slots).
- Allocation = take from the cache's "partial" slab (one with free slots). Free = push back on freelist.

```c
typedef struct kv_slab_cache {
    const char    *name;          // "proc_cache", "vma_cache", etc.
    size_t         object_size;
    size_t         align;
    size_t         objects_per_slab;
    void         (*ctor)(void *);  // optional construct-on-alloc
    void         (*dtor)(void *);  // optional destruct-on-free
    struct list_head slabs_full;
    struct list_head slabs_partial;
    struct list_head slabs_empty;
    kv_spinlock_t  lock;
    atomic_t       in_use;        // active objects
    atomic_t       total;         // capacity
} kv_slab_cache_t;

typedef struct kv_slab {
    kv_slab_cache_t *cache;
    void           *base;
    uint32_t        free_count;
    uint32_t        first_free;   // index of first free slot
    uint32_t       *freelist;     // small in-band index array
    struct list_head node;        // chain in cache's lists
} kv_slab_t;

// API
kv_slab_cache_t *kv_slab_create(const char *name, size_t size, size_t align,
                                void (*ctor)(void*), void (*dtor)(void*));
void   *kv_slab_alloc(kv_slab_cache_t *);
void    kv_slab_free(kv_slab_cache_t *, void *);
void    kv_slab_destroy(kv_slab_cache_t *);
```

**Why object_size matters:** Every type gets one cache. Examples:
- `proc_cache` — ~512 bytes
- `vma_cache` — ~64 bytes
- `file_cache` — ~128 bytes
- `inode_cache` — ~256 bytes
- `pipe_cache` — ~256 bytes
- `mbuf_cache` — 2048 bytes (packet buffer size)
- `trace_payload_cache` — 256 bytes

### 2.3 The ctor/dtor — why DSSI needs them

A typical kernel uses constructors for "field zeroing." But for DSSI checkpoint/restore, ctor/dtor become much more interesting:

- **On `kv_slab_alloc`**, the ctor can register the new object with a per-type **liveness tracker** — used at checkpoint time to enumerate all live objects of each type.
- **On `kv_slab_free`**, the dtor unregisters from the tracker.

This means: "snapshot every live `vma_t`" is a single walk of `vma_cache`'s liveness list, not a tree walk of every process.

```c
static struct list_head proc_live_list;

static void proc_ctor(void *obj) {
    proc_t *p = obj;
    memset(p, 0, sizeof(*p));
    list_add(&p->live_node, &proc_live_list);   // register liveness
}

static void proc_dtor(void *obj) {
    proc_t *p = obj;
    list_del(&p->live_node);
}
```

This pattern is one of the most powerful tools for any kernel that wants to be introspectable.

### 2.4 kmalloc / kfree on top of slabs

For variable-sized allocations, use a set of "size classes" — predefined slab caches at power-of-two sizes:

```c
static kv_slab_cache_t *size_caches[16];   // 16, 32, 64, ..., up to 64K

void *kmalloc(size_t size) {
    if (size > 64 * 1024) {
        // fall back to direct page allocation
        return page_alloc_contiguous(SIZE_TO_PAGES(size));
    }
    int class = size_class_for(size);
    return kv_slab_alloc(size_caches[class]);
}

void kfree(void *ptr) {
    // figure out which cache from object header or page metadata
    kv_slab_cache_t *c = slab_cache_of(ptr);
    kv_slab_free(c, ptr);
}
```

**Where to store the slab-of-origin pointer?** Two choices:
- **Per-object header** (8 bytes prepended) — simple, wastes a bit
- **Per-page metadata** (every page knows its slab via a side table) — Linux SLUB does this

For a first implementation, the per-object header is fine.

### 2.5 Memory zones

Even at this phase, distinguish zones:

```c
typedef enum kv_mem_zone {
    KV_ZONE_NORMAL,        // general kernel use
    KV_ZONE_DMA,           // < 16 MB (legacy ISA DMA); also low-32-bit for old NICs
    KV_ZONE_USER,          // pages eligible for user mapping
    KV_ZONE_FABRIC,        // future: pages staged for fabric transfer
} kv_mem_zone_t;
```

Most allocations are `KV_ZONE_NORMAL`. The `KV_ZONE_FABRIC` zone is reserved for Phase 18+ when you stage outgoing migration buffers — keeping them physically contiguous and pinned helps zero-copy NIC transfer.

### 2.6 Slab debugging features

Add from day one (cost is one config flag):
- **Poisoning**: freed objects filled with `0xDEADBEEF` — use-after-free crashes loudly
- **Red zones**: bytes immediately before and after each object set to a magic value, checked on free
- **Allocation tracking**: each object remembers `(file, line, function)` of last allocator — diagnostic gold
- **Leak detection**: at shutdown (or on demand), report any objects still in use

The cost is ~16 extra bytes per object and ~5% perf overhead. **Always on during development.** Compile out for production.

### 2.7 Per-CPU magazines (future-proofing)

When you add SMP (Phase 25+), allocating from a global slab causes lock contention. The trick: each CPU keeps a small "magazine" (per-CPU free list) of recently-freed objects. Most alloc/free hits the magazine — no lock.

Don't implement now. Just design the API so it can slide in: `kv_slab_alloc(cache)` should be a single inline function that you can later refactor to check a per-CPU magazine first.

---

## 3. Phase 15 Overview — What Checkpoint Means

### 3.1 The precise definition

A **checkpoint** is a serialized representation of a process's complete state, sufficient that:

```
P                                  P'
│                                  │
│  checkpoint                      │  restore from same image
│  (kill original)                 │  
▼                                  ▼
[saved image]  ────────────►       [P' identical to P at instant of checkpoint]
```

After restore, P' must:
- Have the same register values
- Have the same memory contents (every page, every byte)
- Hold the same file descriptors pointing to the same offsets in the same files
- Have the same pending signals queued
- Be in the same wait state if it was blocked
- Resume execution at the same instruction
- Be indistinguishable to userspace from the original

This is hard.

### 3.2 What "indistinguishable" allows us to skip

A few things are allowed to differ:
- **PID** may change (but ideally stays the same when restored on the same node)
- **Timestamps** of in-progress operations may shift slightly (TSC re-baselined)
- **Pointers in pages may be valid** because we restore at the same virtual addresses (this is critical)

What's NOT allowed to differ:
- Anything reachable via syscalls
- Anything in process memory
- Anything observable to the program

### 3.3 The local-only contract for Phase 15

Phase 15 builds checkpoint/restore as a **purely local operation**. No network. The image is staged in a kernel memory buffer (or eventually a local file). Restore happens on the same node.

This sounds limited. It is enormously valuable:
- **Process snapshots** — debugging gold: snapshot at the moment of a bug, restore later to reproduce
- **Fast process spawn** — checkpoint a freshly-initialized shell, spawn new shells by restore (CRIU does this for containers)
- **Crash recovery** — periodic checkpoints, recover the latest after a crash
- And: **every later DSSI phase is "checkpoint + transport + restore."** Get this right, the rest follows.

---

## 4. Prior Art: CRIU and What It Teaches Us

### 4.1 CRIU (Checkpoint/Restore In Userspace)

The reference implementation for Linux. ~100k lines of code. Used in production by Google, Microsoft (for Live Migration of WSL2), container runtimes (Podman, Docker checkpointing).

**What CRIU got right:**
- **Image format is files**, not one monolithic blob — `pages.img`, `core-PID.img`, `fdinfo.img`, etc. Modular, debuggable, partial-restore-friendly.
- **Restore is a "process zoo": multiple helper processes** cooperate to rebuild structures, then the final tree merges. Avoids restoring-while-modifying paradoxes.
- **Checks for "checkpoint dump-ability"** before starting — fails fast if process holds unsupported state (e.g., open BPF maps).
- **Hooks for unsupported features** — you can write plugins for new state types without touching core CRIU.

**What CRIU got hard:**
- Linux has decades of accreted state types. Some take hundreds of lines just to enumerate.
- Process namespaces, cgroups, BPF programs, SELinux contexts, eBPF map FDs, GPU FDs — each one is its own subsystem.

**What Kernel-V can do differently:**
- We control the kernel. We can *design* state to be checkpointable from day one (which Document 01 already started — VMAs as first-class, structured wait state, queued signals).
- We have a much smaller surface area. The seeds make us ready.
- We do C/R *in the kernel* (CRIU does it in userspace via `ptrace`/`/proc`). In-kernel is simpler, faster, and avoids the userspace-kernel chicken-and-egg.

### 4.2 Other prior art worth knowing

- **DMTCP (Distributed MultiThreaded Checkpointing)** — userspace, language-agnostic, used in HPC
- **BLCR (Berkeley Lab Checkpoint/Restart)** — kernel module, simpler, deprecated
- **Singularity / container checkpointing** — uses CRIU under the hood
- **Microsoft Project Freta** — VM-level memory snapshotting for forensics

For Kernel-V: **CRIU's image format is the design reference. Its implementation choices are not.** In-kernel is the way.

---

## 5. The Checkpoint Image Format

### 5.1 Design principles

- **Self-describing**: every chunk has a magic + type so we can parse without external schema
- **Versioned**: include format version for future compatibility
- **Chunked**: separate sections for separate state types (parallel restore possible)
- **Streamable**: can be written/read sequentially (no seeking required) — important for fabric streaming later
- **Checksummed**: integrity check per chunk and overall

### 5.2 Structure

```
┌──────────────────────────────────────────────────────┐
│ HEADER (fixed 256 bytes)                              │
│   magic = "KVCP"                                      │
│   version, flags, timestamp                           │
│   source node_id, source pid, trace_id                │
│   total_size, chunk_count, checksum                   │
├──────────────────────────────────────────────────────┤
│ CHUNK: PROCESS_META                                   │
│   pid, ppid, state, gid, trace_id, ledger snapshot    │
├──────────────────────────────────────────────────────┤
│ CHUNK: REGISTERS                                      │
│   trapframe (eax..edi, eip, eflags, cs/ds/...)        │
├──────────────────────────────────────────────────────┤
│ CHUNK: KERNEL_STACK                                   │
│   raw bytes of the kernel stack                       │
├──────────────────────────────────────────────────────┤
│ CHUNK: VMA_LIST                                       │
│   array of (start, end, prot, kind, backing) records  │
├──────────────────────────────────────────────────────┤
│ CHUNK: PAGES                                          │
│   for each present page: (vaddr, flags, [data])       │
│   compressed (LZ4)                                    │
├──────────────────────────────────────────────────────┤
│ CHUNK: FD_TABLE                                       │
│   array of (fd_num, type, file_meta) records          │
├──────────────────────────────────────────────────────┤
│ CHUNK: OPEN_FILES                                     │
│   per-file: path, flags, position, lock state         │
├──────────────────────────────────────────────────────┤
│ CHUNK: PENDING_SIGNALS                                │
│   queue of pending signals                            │
├──────────────────────────────────────────────────────┤
│ CHUNK: WAIT_STATE                                     │
│   if blocked: wait reason, target gid                 │
├──────────────────────────────────────────────────────┤
│ CHUNK: NAMESPACE                                      │
│   namespace contents (mounts, etc.)                   │
├──────────────────────────────────────────────────────┤
│ CHUNK: TRAILER                                        │
│   final checksum, end marker                          │
└──────────────────────────────────────────────────────┘
```

### 5.3 Chunk header

Every chunk:

```c
typedef struct kv_ckpt_chunk_hdr {
    uint32_t  magic;           // 0x4348 ('CH')
    uint32_t  type;            // KV_CHUNK_PROCESS_META, etc.
    uint64_t  uncompressed_size;
    uint64_t  compressed_size;  // 0 if not compressed
    uint32_t  compression;      // 0=none, 1=LZ4, 2=ZSTD
    uint32_t  checksum;
    // body follows
} kv_ckpt_chunk_hdr_t;
```

### 5.4 Page chunk encoding

The PAGES chunk is by far the largest. Encoding matters:

```c
typedef struct kv_ckpt_page_record {
    uint64_t  vaddr;           // virtual address in process
    uint32_t  flags;            // page flags (writable, etc.)
    uint32_t  data_len;         // 0 = zero page (just record vaddr)
    // raw page data follows (4096 bytes if data_len==4096)
} kv_ckpt_page_record_t;
```

**Optimization: zero-page elision.** If a page contains all zeros, store only the record (no data). Hugely common: BSS, fresh heap, sparse mmaps. Often 30-50% of a process's pages are zero.

**Optimization: dedup on identical pages.** Use a hash table during checkpoint; if a page hashes to one already seen, emit a back-reference. Catches mmap'd files, shared libraries, etc. KSM (Kernel Samepage Merging) is the inspiration.

---

## 6. Freezing a Process Safely

You cannot checkpoint a process while it's running — registers and memory change between samples.

### 6.1 The freeze state

Add a new process state:

```c
typedef enum proc_state {
    PROC_NEW,
    PROC_RUNNABLE,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_FROZEN,        // NEW: checkpoint in progress, do not schedule
    PROC_ZOMBIE,
    PROC_DEAD,
} proc_state_t;
```

The scheduler skips `PROC_FROZEN` processes.

### 6.2 The freeze sequence

```
1. Mark proc as FROZEN
2. If currently running: wait for it to enter kernel (next syscall or timer)
3. If blocked: capture the blocked state, mark it OK to checkpoint
4. Drain in-flight I/O for this process (wait for completions)
5. Quiesce: ensure no kernel thread is touching this proc's structures
6. Take a snapshot of registers + stack (trapframe is canonical source)
7. Now safe to walk address space, FDs, etc.
```

### 6.3 The "kernel-stack roach motel" problem

A process may be in the middle of a syscall when frozen. Its kernel stack contains live state: locals, return addresses, intermediate values. You **cannot** simply throw the kernel stack away — when restored, the process must continue from exactly where it was.

**Solution:** The kernel stack is preserved verbatim. On restore, you load the saved stack bytes into a freshly-allocated kernel stack at *the same physical layout* (you re-execute from the same return address). The kernel stack thus appears unchanged to the process.

**Implication:** All kernel functions touched while a frozen process's syscall is in progress **must not store pointers to other processes' state on their stack**. Otherwise the saved stack contains stale pointers that may not exist after restore.

**For Kernel-V's scope:** Phase 15 only checkpoints processes that are *not in the middle of a kernel syscall*. Either runnable, sleeping in user mode, or blocked at a well-defined syscall entry point. This drops the kernel-stack-roach-motel problem entirely.

The "freeze a process mid-syscall" feature is deferred to Phase 22+ (deep migration). 90% of the value is captured without it.

### 6.4 Async I/O draining

If the process submitted async block I/O before being frozen, those operations might complete *after* the checkpoint. The completions would land in a non-existent process.

Solution (the easy one for Phase 15): wait for all in-flight I/O for this process to complete before checkpointing. Use the per-process I/O tracking from Document 01 Seed 1.4. Drain time is typically milliseconds.

### 6.5 Atomicity guarantee

The whole freeze-snapshot-emit sequence must be atomic *from the process's perspective*. Achieved by holding the process structure's mutex from freeze through snapshot completion. Other processes can run; only the target is paused.

---

## 7. What to Serialize (the Inventory)

### 7.1 Registers and trapframe

```c
typedef struct kv_ckpt_registers {
    // General-purpose
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    // Control
    uint32_t eip, eflags;
    // Segments
    uint16_t cs, ds, es, fs, gs, ss;
    // (Future) FPU state: 512 bytes for FXSAVE
    uint8_t  fpu_state[512];
    // (Future) SSE/AVX state
} kv_ckpt_registers_t;
```

Source: the trapframe on top of the kernel stack, which is captured during freeze.

### 7.2 Memory regions and contents

For each `kv_vma_t` in the process:
- Record the VMA metadata (start, end, prot, kind)
- Walk the page table for that range
- For each *present* page, emit a page record (with data, or zero-elided)
- For each *absent* page (demand-paged, not yet faulted in), emit nothing — restore will fault it back

Per-process pages typically: 1k-100k. Each page record is 4KB + ~16 bytes header. A 100 MB process = 25k pages = ~100 MB image uncompressed.

### 7.3 File descriptors

For each open FD:

```c
typedef struct kv_ckpt_fd {
    uint32_t  fd_num;
    uint32_t  flags;        // O_RDONLY, O_NONBLOCK, etc.
    off_t     position;
    uint64_t  file_gid_node;
    uint64_t  file_gid_local;
    // For paths: char path[256] if it's a regular file
    char      path[256];
    uint32_t  type;         // FD_FILE, FD_PIPE, FD_DEVICE
    // Type-specific fields appended
} kv_ckpt_fd_t;
```

**The key insight:** For local restore, the file may still exist (we didn't delete it). For remote restore (later phases), we need to *reopen* the file by path — which requires a path-resolvable identifier.

Phase 15 stores the path. Phase 18+ adds richer identifiers.

### 7.4 Pending signals queue

Recall Document 01 Seed 6.2: signals are queued, not delivered direct-to-registers. Serialization is then:

```c
typedef struct kv_ckpt_signal_queue {
    uint32_t   count;
    kv_signal_t signals[];     // count entries
} kv_ckpt_signal_queue_t;
```

### 7.5 Wait state

From Document 01 Seed 6.3:

```c
typedef struct kv_ckpt_wait {
    uint32_t   reason;          // KV_WAIT_PIPE_READ, etc.
    uint64_t   target_gid_node;
    uint64_t   target_gid_local;
    uint64_t   wait_start_tsc;
} kv_ckpt_wait_t;
```

On restore, the process is added back to the appropriate wait queue. If the target no longer exists (e.g., the pipe was destroyed), restore fails cleanly.

### 7.6 Process metadata

PID, PPID, UID/GID (if implemented), GID (your global GID), trace_id, ledger snapshot:

```c
typedef struct kv_ckpt_proc_meta {
    kv_gid_t  gid;
    uint32_t  pid, ppid;
    uint32_t  state_at_checkpoint;
    uint64_t  trace_id;
    uint64_t  cpu_ticks_user;
    uint64_t  cpu_ticks_kernel;
    // ... selected ledger fields ...
    uint64_t  checkpoint_timestamp_tsc;
    char      argv[256];        // for human-readable
    char      cwd[256];
} kv_ckpt_proc_meta_t;
```

### 7.7 Namespace

```c
typedef struct kv_ckpt_namespace {
    uint32_t  mount_count;
    // followed by mount records
} kv_ckpt_namespace_t;
```

### 7.8 What NOT to serialize (Phase 15)

- **Open sockets**: defer to Phase 19 (syscall proxy will handle them remotely)
- **Shared memory mappings with other processes**: not supported in Phase 15
- **Memory-mapped device buffers**: would require device cooperation — defer
- **Threads beyond the main one**: single-threaded only in Phase 15
- **GPU context, accelerator state**: pin process to local node, never migrate

---

## 8. Walking the Address Space

The most performance-critical part of checkpoint is walking the page tables. Two approaches:

### 8.1 VMA-driven walk (recommended)

Iterate the process's VMA list (Document 01 Seed 5.1). For each VMA, walk the page table only over its range. Skip absent pages.

```c
list_for_each(vma, &proc->vmas) {
    emit_vma_metadata(vma);
    for (vaddr = vma->start; vaddr < vma->end; vaddr += PAGE_SIZE) {
        pte_t *pte = walk_page_table(proc->page_dir, vaddr);
        if (pte_present(pte)) {
            paddr_t phys = pte_to_phys(pte);
            void *page = phys_to_virt(phys);
            if (is_zero_page(page)) {
                emit_page_record(vaddr, 0, NULL);
            } else {
                emit_page_record(vaddr, PAGE_SIZE, page);
            }
        }
    }
}
```

This skips kernel memory entirely (you only walk user VMAs). Fast and clean.

### 8.2 Page-table-driven walk (avoid)

Walk every PDE/PTE in the user portion of the page directory. Inefficient — most slots are empty.

### 8.3 Optimizations

- **Identical-page deduplication**: hash each page (e.g., xxHash or FNV); maintain a hash→offset table. Identical pages emit a back-reference instead of full data.
- **mmap'd executable pages**: if a page maps `/bin/ls` text segment, you don't need to serialize the bytes — record (file, offset, len) and restore by remapping the file. Major space savings.

These optimizations can be deferred to Phase 15.2 or 15.3.

---

## 9. Restore: Reversing the Process

Restore is checkpoint backwards. Each step:

### 9.1 The restore sequence

```
1. Read HEADER, verify magic and version
2. Allocate a new proc_t via slab
3. Create a new page directory (empty)
4. Read PROCESS_META: populate proc fields (pid hint, trace_id, etc.)
5. Read VMA_LIST: create kv_vma_t entries
6. Read PAGES: for each, allocate a frame, copy data, install in page table
7. Read FD_TABLE + OPEN_FILES: reopen files, reconstruct file_t entries
8. Read PENDING_SIGNALS: requeue
9. Read WAIT_STATE: if blocked, add to appropriate wait queue
10. Read NAMESPACE: reconstruct namespace
11. Read REGISTERS + KERNEL_STACK: set up the new process to "return from a syscall"
                                   with the saved register values
12. Mark RUNNABLE
13. Scheduler picks it up; it continues execution at saved EIP
```

### 9.2 The restore "trampoline"

A restored process must enter user mode at the saved EIP with the saved registers. Mechanism: build a fake trapframe on the new kernel stack containing the saved registers, then `iret` out of the kernel to user mode. Same trick you already use for fresh process startup.

```c
void restore_to_user(proc_t *p, kv_ckpt_registers_t *regs) {
    trapframe_t *tf = (trapframe_t *)(p->kstack_top - sizeof(trapframe_t));
    tf->eax = regs->eax;
    tf->ebx = regs->ebx;
    // ... all registers ...
    tf->eip = regs->eip;
    tf->eflags = regs->eflags;
    tf->cs  = USER_CS;
    tf->ss  = USER_SS;
    tf->esp = regs->esp;
    // Process is now ready: scheduler->switch_to(p) will iret to user mode
}
```

### 9.3 Re-establishing virtual addresses

Pages are restored at the **same virtual addresses** they had in the source. This is critical: pointers in the saved data are valid because the virtual mapping is identical.

For this to work locally, the source process must be killed *before* the restored process starts using its address space — or you'll have two procs with the same VAs and the scheduler will pick one or the other but not get confused (each has its own page directory).

### 9.4 FD re-establishment

For each saved FD:
- If it's a regular file, `open(path, flags)` with saved flags
- Set position to saved offset
- Install in new proc's FD table at saved fd_num

If the file no longer exists on the local filesystem, restore fails cleanly with an error chunk.

### 9.5 Restore as a kernel thread

The restore operation runs as a kernel thread, not as the new process itself. The new process is built, then handed to the scheduler. Cleanly separates "construction" from "execution."

### 9.6 Partial restore

If any chunk fails to deserialize, the partial proc is freed via slab. No half-restored zombies. Use a transaction pattern: build into a new proc, validate completely, atomically install into proc table only when validated.

---

## 10. Compression

### 10.1 Why compress

For Phase 15 (local C/R), compression saves memory in the staged image. For Phase 18+ (network transfer), compression dramatically reduces bytes on the wire.

Typical compression ratios on real process memory:
- LZ4 (fast, low ratio): 2-3×
- ZSTD level 1 (fast, medium): 3-4×
- ZSTD level 9 (slow, high): 4-6×

A 100 MB process compresses to ~30 MB with LZ4 — and LZ4 runs at ~4 GB/s on a modern CPU. The compression is *faster* than transferring uncompressed bytes over 25 GbE.

### 10.2 Per-chunk compression

The chunk header has a `compression` field. Each chunk independently compressed. Allows mixing — small chunks uncompressed, big chunks compressed.

### 10.3 LZ4 implementation

LZ4 has a well-defined, small reference implementation (~500 lines). Port it into Kernel-V's libk. Public domain (BSD).

For ZSTD, defer until needed — much bigger codebase.

### 10.4 Compression in pipelined fashion

Don't compress the whole image then send. Compress chunk-by-chunk into a ring buffer, send/store as it produces. Pipelined = lower latency, lower peak memory.

---

## 11. Testing Strategy

This phase needs aggressive testing because everything later depends on it.

### 11.1 The identity test

The fundamental correctness test:

```c
void test_checkpoint_identity(void) {
    proc_t *p = spawn_test_proc(test_workload);
    sleep_ticks(100);  // let it do some work

    uint8_t *image;
    size_t image_len;
    int rc = checkpoint(p, &image, &image_len);
    ASSERT(rc == 0);

    // Snapshot the process's externally visible state
    proc_observable_t before = observe(p);

    // Restore into a new proc, kill the old
    proc_t *p2 = restore(image, image_len);
    kill_proc(p);
    proc_observable_t after = observe(p2);

    ASSERT_EQ(before.eip, after.eip);
    ASSERT_EQ(before.cwd, after.cwd);
    ASSERT_MEMORY_EQ(before.pages, after.pages);
    ASSERT_FDS_EQ(before.fds, after.fds);
}
```

Run this for many workloads:
- CPU loop (no I/O)
- Memory-heavy (allocates and writes 50 MB)
- File-heavy (opens many files)
- Pipe-using (reads from a pipe)
- Recently-forked child

### 11.2 The round-trip test

Checkpoint → modify image trivially → restore → verify difference shows up. Catches checkpoint/restore code that ignores the image.

### 11.3 The continuation test

The process should keep running correctly after restore:

```c
void test_checkpoint_continuation(void) {
    proc_t *p = spawn_test_proc(counter_workload);  // counts up

    // Wait for it to reach count = 100
    wait_for_counter(p, 100);

    uint8_t *image;
    size_t  image_len;
    checkpoint(p, &image, &image_len);
    kill_proc(p);

    proc_t *p2 = restore(image, image_len);

    // Wait for restored proc to reach count = 200 (100 more)
    wait_for_counter(p2, 200);
    // If it correctly resumed from 100, this works
}
```

### 11.4 The mid-blocked test

Test a process blocked on a pipe read:

```c
void test_checkpoint_blocked(void) {
    pipe_t *p = make_pipe();
    proc_t *reader = spawn_proc(read_from_pipe, p);
    wait_for_state(reader, PROC_BLOCKED);

    uint8_t *image; size_t len;
    checkpoint(reader, &image, &len);

    proc_t *r2 = restore(image, len);
    ASSERT(r2->state == PROC_BLOCKED);
    ASSERT(r2->blocked_on.target.local_id == p->id.local_id);

    // Write to pipe; verify r2 wakes up
    pipe_write(p, "hello", 5);
    wait_for_state(r2, PROC_RUNNABLE);
}
```

### 11.5 Stress and chaos tests

- **Loop test**: checkpoint and restore 1000 times in a tight loop, verify no leaks
- **Resource exhaustion**: try to checkpoint when low on memory; should fail cleanly
- **Concurrent checkpoint**: two processes checkpointed simultaneously, verify both work
- **Corrupted image**: hand-edit bytes in the image, verify restore fails cleanly without crashing

### 11.6 Property-based tests

Generate random processes (random heap content, random FD tables) and assert: `restore(checkpoint(p)) ≡ p` for all p.

---

## 12. Known Limitations & Scope

What Phase 15 **does** support:
- Single-threaded processes
- Open files (regular files only)
- Pipes (both ends)
- Pending signals
- Blocked-on-pipe / blocked-on-wait state
- Anonymous mmap and heap
- File-backed mmap (read-only)

What Phase 15 **does NOT** support:
- Multi-threaded processes (Phase 22+)
- Sockets (Phase 19 — handled via syscall proxy)
- Shared memory between processes (Phase 22+)
- File-backed mmap with `MAP_SHARED` writeback
- Mid-syscall checkpoint (Phase 22+)
- Memory-mapped devices
- GPU state

**Documented behavior on unsupported state**: `checkpoint()` returns `-EUNSUPPORTED` with a chunk in the error log explaining what type of state caused the rejection. The process continues running normally. No silent data loss.

---

## 13. Implementation Phasing

### Phase 14 (3-4 weeks)
- 14.1 Slab allocator core (cache create, alloc, free)
- 14.2 kmalloc/kfree on top of slabs
- 14.3 Ctor/dtor + per-type liveness lists
- 14.4 Memory zones
- 14.5 Slab debugging (poison, redzone, tracking)
- 14.6 Migrate existing kernel allocations to use slabs

### Phase 15 (8-10 weeks)
- 15.1 Image format and chunk infrastructure
- 15.2 Process freeze mechanism
- 15.3 Register + stack serialization
- 15.4 VMA + page serialization (no compression yet)
- 15.5 LZ4 integration
- 15.6 FD + open-file serialization
- 15.7 Signal queue + wait state serialization
- 15.8 Local restore (mirror of checkpoint)
- 15.9 Identity test, continuation test, blocked test
- 15.10 `checkpoint` / `restore` shell commands
- 15.11 Stress tests + leak checks
- 15.12 Phase 15 contract document

**Total: ~13 weeks for Phases 14 + 15.** Real life: pad to 15-16 weeks. This is the most subtle code in the entire DSSI effort.

---

## 14. Success Criteria

End-of-Phase-14 demo:
```sh
> slab stats
Cache               Objects     Slabs    Pages
proc_cache          5           1        1
vma_cache           23          1        1
file_cache          17          1        1
inode_cache         42          1        2
mbuf_cache          0           0        0

> kmalloc test --leak
PASS: 10000 alloc/free pairs, 0 leaked
```

End-of-Phase-15 demo:
```sh
> /bin/counter &
[pid 7] started, counting...
> checkpoint pid 7 > /tmp/counter.snap
Checkpoint complete: 12 chunks, 47 KB image (3.2x compression)
> kill 7
> restore /tmp/counter.snap
[pid 8] restored from /tmp/counter.snap, continuing...
[pid 8] counter=101  <- continues from where pid 7 was!
[pid 8] counter=102
```

After this milestone, you have **the single hardest primitive in DSSI working** — locally. Every later phase is about transporting the image across the network instead of writing it to a file.

---

## Closing Note

This document specifies the *most technically demanding* phase of the entire DSSI journey. Take the time. Build slowly. Test ruthlessly.

Three rules to internalize:

1. **No mid-syscall checkpoint in Phase 15.** Defer the kernel-stack-roach-motel to Phase 22+. You will save months.
2. **Test with the identity test before everything else.** If `restore(checkpoint(p)) ≡ p` doesn't hold, nothing else matters.
3. **Image format is forever.** A bad image format means every Kernel-V version after this can't read old checkpoints. Get the chunk-based, self-describing, versioned format right. Add fields later, never re-purpose them.

If you finish this phase with a working `checkpoint pid X; kill X; restore /tmp/snap` flow, the network can come anytime — you'll have the hard problem behind you.

---

**Next document to write:** `dssi_04_fabric_control_plane.md` — Phases 16-17. Fabric node identity, peer discovery, gossip protocol, Phi Accrual failure detection, resource advertisement. The "management layer" you described in your original vision starts taking concrete shape.
