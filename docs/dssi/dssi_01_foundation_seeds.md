# Kernel-V DSSI — Document 01: Foundation Seeds (Phases 6-11)

> **DSSI-aware design decisions to inject into your existing foundation phases.**
> Small additions you make *now*, while building the UNIX-like foundation, that will save *months* of rework when the fabric layer arrives.

This document is the most immediately actionable in the eight-document series. You are entering Phase 6 next. The decisions you make in Phases 6-11 will either help or hurt DSSI later. This document tells you which small choices matter and why.

**Reading guidance:** If you only have time for one section right now, read [Section 1: Cross-Cutting Seeds](#1-cross-cutting-seeds) — those apply to every phase and are zero-cost to add up front, but expensive to retrofit.

---

## Table of Contents

1. [Cross-Cutting Seeds (apply to every phase)](#1-cross-cutting-seeds)
2. [Phase 6 — Driver Model Seeds](#2-phase-6--driver-model-seeds)
3. [Phase 7 — Block I/O & Buffer Cache Seeds](#3-phase-7--block-io--buffer-cache-seeds)
4. [Phase 8 — VFS Seeds](#4-phase-8--vfs-seeds)
5. [Phase 9 — Userland & ELF Seeds](#5-phase-9--userland--elf-seeds)
6. [Phase 10 — IPC Seeds](#6-phase-10--ipc-seeds)
7. [Phase 11 — Networking Seeds](#7-phase-11--networking-seeds)
8. [The DSSI-Readiness Checklist](#8-the-dssi-readiness-checklist)
9. [What to Resist Adding](#9-what-to-resist-adding)

---

## 1. Cross-Cutting Seeds

These seven principles apply across every phase 6-11. Internalize them once.

### 1.1 Every kernel object gets a stable, serializable identity

**The rule:** Anything that may one day need to be referenced from a remote node — process, file, pipe, socket, device, inode — must have an identifier that is **unique across the fabric** and **serializable** (representable as bytes that can go over the wire).

**The pattern:** A globally unique ID is `(node_id, local_id)` where `node_id` is a 64-bit value assigned at fabric join, and `local_id` is whatever you already use locally.

```c
// kernel/include/kv_gid.h
typedef struct kv_gid {
    uint64_t node_id;     // 0 means "local node" (filled in later)
    uint64_t local_id;    // current pid_t, fd_t, inode_num, etc.
} kv_gid_t;
```

**What to do now in Phase 6-11:**
- Every kernel object (`proc_t`, `file_t`, `inode_t`, `pipe_t`, `kv_device_t`) gets a `kv_gid_t id;` field.
- For now, `node_id` is always `0` (meaning "this node"). The field exists but is unused.
- Format every diagnostic line with `gid` instead of bare `pid`: `[pid=0:42]` instead of `[pid=42]`.
- Comparison helpers: `kv_gid_equal()`, `kv_gid_is_local()`.

**Cost now:** ~50 lines of code. Header file, helpers, a few extra struct fields.
**Savings later:** Avoiding a massive sweep through the whole kernel to add node_id fields when fabric arrives.

### 1.2 Every kernel object can be dumped to a stable byte format

**The rule:** Every important kernel object must support serialization. Not for performance — for *checkpointability*. Phase 15 (process checkpoint) absolutely depends on this; you save weeks if every subsystem has serialize hooks from day one.

**The pattern:** Each object type has two functions:

```c
// Returns number of bytes written, or -1 on error.
// If buf is NULL, returns required buffer size without writing.
ssize_t obj_serialize(const obj_t *obj, void *buf, size_t bufsize);

// Reconstruct from bytes. Returns 0 on success.
int obj_deserialize(obj_t *obj, const void *buf, size_t bufsize);
```

**What to do now:** Whenever you add a new kernel struct, also stub out these two functions — even if they just write/read a fixed header for now. Later you fill them in.

**Cost now:** Trivial.
**Savings later:** Phase 15 becomes "wire up the existing serialize functions" instead of "write serialization for 20 subsystems from scratch."

### 1.3 No synchronous network assumptions in the locking model

**The rule:** When you design the lock hierarchy for any subsystem, assume that any operation on a non-local object **may take milliseconds**. Never hold a spinlock or disable interrupts across an operation that could one day cross the network.

**Concrete:** If `file_read(file_t *f)` will one day proxy to a remote node, then **do not call it while holding a spinlock**. Use sleeping locks (mutexes) for VFS, IPC, networking, etc. — not spinlocks.

**What to do now:**
- Make a clean distinction in your synchronization library: `kv_spinlock_t` (short, no sleep) vs `kv_mutex_t` (can sleep). You probably already have spinlocks; add mutexes in Phase 6 before they're needed.
- Lock-discipline document: which subsystems use which lock type. VFS, IPC, fabric → mutexes. PMM, scheduler ready-queue, IRQ paths → spinlocks.
- Build a `lockdep`-style debugging tool (Phase 12 territory) that warns if a spinlock is held across a potentially-sleeping call.

**Cost now:** Designing the mutex API and lock hierarchy. ~1 week of careful thinking.
**Savings later:** Avoiding the nightmare of "the fabric proxies a syscall, but the syscall path holds an irq-off spinlock, so the whole kernel deadlocks." This bug, if not prevented architecturally, takes *months* to root-cause later.

### 1.4 Async-first, sync-as-shortcut

**The rule:** I/O subsystems (block, network, fabric, file) should be designed **callback-based / completion-port-style** from day one. A blocking wrapper is fine on top, but the *primitive* must be async.

**The pattern (io_uring-inspired):**

```c
typedef void (*kv_io_completion_fn)(void *cookie, int result);

// Submit an I/O request. Returns immediately.
// completion is called when the I/O finishes.
int block_read_async(block_device_t *bd, uint64_t lba, void *buf,
                     uint32_t count, kv_io_completion_fn completion,
                     void *cookie);

// Convenience blocking wrapper — built ON TOP of the async path.
int block_read(block_device_t *bd, uint64_t lba, void *buf, uint32_t count) {
    sync_helper_t s = { .done = false };
    block_read_async(bd, lba, buf, count, sync_callback, &s);
    sync_wait(&s);
    return s.result;
}
```

**Why this matters:** Migration must be able to "drain in-flight I/Os" before transferring a process. If your I/O API is synchronous, there's no concept of "in-flight" — the syscall is blocked on the stack and you cannot inspect it.

**What to do now:** Phase 7 (block I/O) and Phase 11 (networking) should be designed with the async API as the truth and sync as a wrapper.

**Cost now:** A bit more thinking up front.
**Savings later:** Migration of a process in the middle of an I/O syscall becomes tractable.

### 1.5 Per-process namespaces (Plan 9 inspiration)

**The rule:** Every process should have its own view of the filesystem namespace, device namespace, and (eventually) the fabric namespace. This is exactly Plan 9's model and it is *extraordinarily* powerful.

**The pattern:** Instead of one global mount table, each `proc_t` has a `namespace_t` pointer. Most processes share the parent's namespace. But the design *allows* a process to have a private view.

```c
typedef struct kv_namespace {
    refcount_t refs;
    mount_table_t *mounts;       // file system mounts
    device_table_t *devices;     // visible devices
    fabric_view_t *fabric;       // future: which fabric peers are visible
} kv_namespace_t;

typedef struct proc {
    // ... existing fields ...
    kv_namespace_t *ns;          // most procs share parent's; some have their own
} proc_t;
```

**Why this matters for DSSI:**
- A migrated process can have a namespace that points back to the home node's mounts — *automatically* enabling syscall proxy without any per-syscall logic.
- Security: you can spawn a guest process with a restricted namespace that sees only the resources it's allowed to access.
- Per-node mounts (Plan 9-style): mount `/dev/cpu` to mean "the local CPU stats" on each node, and a migrated process sees its host's stats, not its home's.

**Cost now:** A `namespace_t` indirection level on every namespace access. Most processes share one global default namespace, so the overhead is one pointer dereference.
**Savings later:** The fabric's "transparent file access" feature becomes a namespace trick instead of a per-syscall hack.

### 1.6 Event emission goes everywhere from day one

**The rule:** Every state transition, every resource allocation, every syscall, every interrupt — emits a TraceOS event. Even if you have no TraceOS yet, define the macro and put it everywhere.

**The pattern:**

```c
// In kernel/include/kv_trace.h:
#define KV_TRACE(category, type, ...) \
    do { /* phase 12 fills this in */ } while (0)

// In every interesting code path:
KV_TRACE(KV_TRACE_PROC, PROC_CREATED, pid, parent_pid);
KV_TRACE(KV_TRACE_VFS,  FILE_OPENED,  fd, inode_gid);
KV_TRACE(KV_TRACE_BLOCK,IO_SUBMITTED, dev, lba, count);
KV_TRACE(KV_TRACE_FABRIC,/* unused yet */ MIGRATION_BEGAN, gid, dst_node);
```

**What to do now:** Define the macro as a no-op. Define a comprehensive event-type enum that includes FABRIC categories already (`KV_TRACE_FABRIC_*`) even though no fabric exists yet. Sprinkle `KV_TRACE` calls liberally.

**Cost now:** Zero runtime cost (macro expands to nothing). A bit of typing.
**Savings later:** Phase 12 (TraceOS) becomes "implement the macro" instead of "go add trace calls everywhere." More importantly, when you're debugging a fabric bug at 2am six months from now, the trace is *already there*.

### 1.7 Design metrics that aggregate naturally

**The rule:** When you build the resource ledger (Phase 13) and fabric resource advertisement (Phase 17), you want metrics that meaningfully *add* across nodes. Design them that way from day one.

**Good metrics (aggregate naturally):**
- `bytes_read_total` — a counter, sums across nodes
- `cpu_idle_ticks` — sums to "total idle CPU on fabric"
- `free_pages` — sums to "total free memory on fabric"
- `processes_runnable` — sums to "fabric load"

**Bad metrics (don't aggregate):**
- `current_cpu_usage_percent` — averages don't make sense across heterogeneous nodes
- `last_boot_time` — has no fabric-level meaning

**What to do now:** When adding a new metric (in Phases 6-11), ask "could this be summed across nodes meaningfully?" If yes, expose it as a counter or gauge. If no, expose it as node-local only.

**Cost now:** Just discipline.
**Savings later:** Fabric scheduler in Phase 23 has the data it needs to make good decisions.

---

## 2. Phase 6 — Driver Model Seeds

Your existing Phase 6 builds the generic device/driver model (match/probe/remove). Three additions that cost little now and matter a lot later:

### 2.1 Devices have proxyability flags

Not every device can be used from a remote process. Encode this in the device model.

```c
typedef enum kv_device_proxy {
    KV_DEV_PROXY_NONE,        // Cannot be used remotely. Process must stay on this node.
                              // (GPUs, real-time hardware, devices with DMA buffers)
    KV_DEV_PROXY_OPS,         // Operations can be proxied (read/write/ioctl).
                              // (block devices, character devices, TTYs)
    KV_DEV_PROXY_STREAM,      // Streaming proxy: data flows over fabric.
                              // (audio devices, network sockets — later)
    KV_DEV_PROXY_FULL,        // Device itself can be migrated as part of process state.
                              // (purely-software pseudo-devices)
} kv_device_proxy_t;

typedef struct kv_device {
    // ... existing fields ...
    kv_gid_t                 id;            // global identity (seed 1.1)
    kv_device_proxy_t        proxy_class;   // how, if at all, this device can be used remotely
    const char              *fabric_name;   // stable name for fabric lookup ("disk:nvme0", "tty:console")
} kv_device_t;
```

**What this enables later:**
- The fabric migration decision-maker checks if a process holds any `KV_DEV_PROXY_NONE` devices. If yes, the process is *unmigratable*. Decision made in microseconds.
- The fabric syscall proxy uses `fabric_name` to find the equivalent device on the home node. "Read from `tty:console`" means "the home node's console," not the host node's.

### 2.2 Device operation table is virtualizable

The driver's ops table (`read`, `write`, `ioctl`) should be the *single dispatch point* for all access. No code path bypasses it. This makes "swap in a proxy ops table when the process is migrated" trivial.

```c
typedef struct kv_device_ops {
    ssize_t (*read)(kv_device_t *dev, void *buf, size_t n, off_t off);
    ssize_t (*write)(kv_device_t *dev, const void *buf, size_t n, off_t off);
    int     (*ioctl)(kv_device_t *dev, unsigned int cmd, void *arg);
    int     (*mmap)(kv_device_t *dev, struct vm_area *vma);
} kv_device_ops_t;
```

**The DSSI win:** When a process holding an FD on device D migrates to node H, the host node's FD table gets an entry whose `ops` table is `proxy_device_ops` — every operation is forwarded over fabric. Same FD interface, different ops table. No process-visible change.

### 2.3 Device resource accounting from day one

Every read/write through a device increments per-device counters that feed the resource ledger:

```c
typedef struct kv_device_stats {
    atomic64_t bytes_read;
    atomic64_t bytes_written;
    atomic64_t ops_count;
    atomic64_t errors;
    // future: latency histograms (Phase 13)
} kv_device_stats_t;
```

This is *zero* additional design overhead — every wrapper just does `atomic_inc()`. But it gives the resource ledger free data, and gives the fabric scheduler "which devices are hot on which nodes" insight.

---

## 3. Phase 7 — Block I/O & Buffer Cache Seeds

Phase 7 is where you build block devices, request submission, buffer cache. Critical phase for DSSI because **storage is one of the most-proxied resources**.

### 3.1 Block addresses are 3-tuples, not 2-tuples

Conventional design indexes blocks as `(device_id, lba)`. DSSI-aware design:

```c
typedef struct kv_block_addr {
    kv_gid_t   device_gid;   // global device identity (which device on which node)
    uint64_t   lba;          // logical block address
} kv_block_addr_t;
```

Locally, `device_gid.node_id == 0` always. But the buffer cache, block request struct, and dirty-block tracker all key on `kv_block_addr_t`, not the bare `(dev, lba)` pair. Later, a "block on node 5's disk" becomes a natural extension.

### 3.2 Buffer cache entries record their lineage

When you add a buffer to the cache, record where it came from:

```c
typedef enum kv_buffer_source {
    KV_BUF_LOCAL_READ,      // read from local block device
    KV_BUF_PREFETCH,        // speculative read-ahead
    KV_BUF_REMOTE_FETCH,    // fetched from a fabric peer (future)
    KV_BUF_DIRTY_WRITE,     // dirtied locally, not yet flushed
} kv_buffer_source_t;

typedef struct kv_buffer {
    kv_block_addr_t addr;
    void           *data;
    uint32_t        flags;
    kv_buffer_source_t source;
    kv_gid_t        last_writer_pid;     // who dirtied this? (for storage-aware sched)
    uint64_t        last_access_tsc;     // when accessed? (for LRU + fabric coordination)
} kv_buffer_t;
```

The `last_writer_pid` and `last_access_tsc` fields are *gold* for the Phase 14 storage-aware scheduler. They cost ~16 extra bytes per buffer.

### 3.3 Block request submission goes through a hookable router

The single function that submits a block I/O request is the choke point. Make it hookable:

```c
// kernel/block/router.c
typedef int (*kv_block_submit_fn)(kv_block_request_t *req);

static kv_block_submit_fn block_submit_hook = block_submit_local;

int block_submit(kv_block_request_t *req) {
    KV_TRACE(KV_TRACE_BLOCK, IO_SUBMITTED, ...);
    return block_submit_hook(req);    // for now, always local
}

// Phase 17+ installs a fabric-aware router:
//   block_submit_hook = block_submit_fabric_aware;
```

This one indirection is the entire seed for "blocks might be on a remote node." When fabric arrives, you replace `block_submit_local` with a router that checks `req->addr.device_gid.node_id` and either runs locally or proxies.

### 3.4 Async block I/O is the primitive (see Seed 1.4)

Already covered. Worth restating: `block_read_async` is the truth, `block_read` is a wrapper. Migration of a process mid-read depends on this.

### 3.5 Dirty-page-style dirty-buffer tracking

Buffers that have been dirtied should be on a per-process list, not a global one. Why? Because the resource ledger (Phase 13) wants to answer "which process is causing storage pressure?", and the fabric needs to "drain a process's dirty buffers before migrating it."

```c
typedef struct proc {
    // ... existing ...
    struct list_head dirty_buffers;   // buffers this process has dirtied
    atomic_t         dirty_buf_count;
    atomic_t         dirty_buf_bytes;
} proc_t;
```

---

## 4. Phase 8 — VFS Seeds

VFS is where the most DSSI-relevant decisions live. Get this right.

### 4.1 File descriptors carry home-node identity

Every open file descriptor records which node "owns" the underlying file. For local processes, this is just self. For migrated processes, this is critical.

```c
typedef struct kv_file {
    kv_gid_t       gid;              // global file identity
    kv_gid_t       home_node;        // where the underlying inode/device lives
    uint32_t       flags;
    off_t          pos;
    kv_file_ops_t *ops;              // local or proxy ops (see seed 2.2)
    void          *private;
} kv_file_t;

typedef struct fd_table {
    kv_file_t *fds[KV_FD_MAX];
} fd_table_t;
```

**For now:** `home_node == kv_self_node_id()` for every file. The field exists but is trivially set.
**Later:** When a process migrates, its FD entries' `home_node` stays pointing to the home node. The fabric syscall proxy uses this field to route reads/writes back.

### 4.2 Inodes have global IDs and version numbers

Even on a single node, give every inode:

```c
typedef struct kv_inode {
    kv_gid_t     gid;           // (node_id, inode_num)
    uint64_t     version;       // monotonic, incremented on every metadata change
    // ... rest of inode fields ...
} kv_inode_t;
```

The `version` field is for *cache coherency across the fabric* — a peer node can ask "do you still have inode (5, 42) at version 17?" and decide whether to refetch.

### 4.3 Path resolution is namespace-scoped

Apply Seed 1.5 here. `path_lookup("/etc/passwd")` first checks `current->ns->mounts`, then walks. This is one indirection now, infinite power later.

### 4.4 Mounted filesystems can declare locality

Each mounted filesystem says whether it is local-only, fabric-shared, or fabric-replicated:

```c
typedef enum kv_fs_locality {
    KV_FS_LOCAL_ONLY,         // /tmp on a ramfs — never shared
    KV_FS_FABRIC_PROXIED,     // shared via syscall forwarding to one node
    KV_FS_FABRIC_REPLICATED,  // replicated across fabric (later, much later)
} kv_fs_locality_t;

typedef struct kv_filesystem {
    const char    *name;
    kv_fs_locality_t locality;
    // ... ops ...
} kv_filesystem_t;
```

For Phase 8, every FS you build is `KV_FS_LOCAL_ONLY`. The enum exists for the future.

### 4.5 File ops have an explicit "can be cached?" hint

Some file operations can be cached by the kernel; some cannot (e.g., reads from a device). Make this explicit:

```c
typedef struct kv_file_ops {
    ssize_t (*read)(kv_file_t *, void *, size_t);
    ssize_t (*write)(kv_file_t *, const void *, size_t);
    int     (*ioctl)(kv_file_t *, unsigned, void *);
    bool    (*is_cacheable)(kv_file_t *);    // hint for fabric: may we cache result?
} kv_file_ops_t;
```

When a guest process does `read()` on a proxied file, the fabric can cache the result locally if `is_cacheable()` returns true — saving round-trips for hot read-mostly files.

---

## 5. Phase 9 — Userland & ELF Seeds

The userspace API is what user processes *see*. Get it right; it's painful to change later.

### 5.1 Track every memory region structurally

When the ELF loader sets up a process, it creates several memory regions: code, data, BSS, heap, stack, mmaped files. Right now these probably live as ad-hoc page mappings. **Make them first-class kernel objects.**

```c
typedef enum kv_vma_kind {
    KV_VMA_TEXT,
    KV_VMA_DATA,
    KV_VMA_BSS,
    KV_VMA_HEAP,
    KV_VMA_STACK,
    KV_VMA_MMAP_FILE,
    KV_VMA_MMAP_ANON,
    KV_VMA_FABRIC_REMOTE,   // future: pages live on another node
} kv_vma_kind_t;

typedef struct kv_vma {
    uintptr_t   start;
    uintptr_t   end;
    uint32_t    prot;       // R/W/X
    kv_vma_kind_t kind;
    kv_file_t  *backing_file;  // for mapped files
    off_t       file_offset;
    struct list_head node;     // chain in proc->vmas
} kv_vma_t;

typedef struct proc {
    // ... existing ...
    struct list_head vmas;   // all memory regions this process owns
} proc_t;
```

**Why this matters:** Phase 15 (checkpoint/restore) walks `proc->vmas` to know what memory to serialize. Without structured VMAs, checkpointing requires walking the entire page directory — much harder.

### 5.2 Syscall numbers are stable and versioned

Define syscall numbers in a *single header file* (`kv_syscalls.h`) and never reuse a number, even if you delete a syscall. The fabric will eventually have version negotiation between nodes — if Node A uses syscall #42 for "open" and Node B uses #42 for "close," fabric breaks.

```c
// kernel/include/kv_syscalls.h
#define KV_SYS_VERSION  1   // bump when syscall ABI changes

#define KV_SYS_EXIT      1
#define KV_SYS_FORK      2
#define KV_SYS_READ      3
#define KV_SYS_WRITE     4
// ... never reuse a number ...
#define KV_SYS_DELETED_5 5  // do not reuse
#define KV_SYS_OPEN      6
// ...
#define KV_SYS_FABRIC_MIGRATE  200   // reserve range 200+ for fabric syscalls
```

### 5.3 libc uses portable wrappers, never inline syscalls

Every libc function goes through a single inline that does the syscall:

```c
// user/libc/syscall.h
static inline long kv_syscall(long num, long a, long b, long c, long d, long e) {
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e));
    return ret;
}
```

Never let user code do `int $0x80` directly. This means later we can rewrite `kv_syscall` to do *anything* — local syscall, fabric syscall, traced syscall — without recompiling user binaries.

### 5.4 Reserve a syscall range for fabric primitives

In `kv_syscalls.h`, reserve a numeric range (e.g., 200-255) for fabric operations. Don't implement them yet. Just reserve.

```c
#define KV_SYS_FABRIC_BASE     200
#define KV_SYS_FABRIC_INFO     200  // get info about local node / fabric
#define KV_SYS_FABRIC_PEERS    201  // list visible peers
#define KV_SYS_FABRIC_MIGRATE  202  // request migration of self/other
#define KV_SYS_FABRIC_PIN      203  // declare unmigratable
// ... up to 255 ...
```

### 5.5 The shell ships with a `fabric` command stub

In Phase 9 when you build the shell, add a `fabric` command that prints "fabric subsystem not yet available." This:
- Forces you to think about the fabric UX from the start
- Means user-facing docs can reference `fabric` from day one
- When fabric ships, no shell change required — the command already exists

---

## 6. Phase 10 — IPC Seeds

IPC is the *primary integration surface* between processes. Every IPC primitive must be designed to one day work across the fabric.

### 6.1 Pipes have endpoint identities

A pipe is two endpoints. Each endpoint knows which process owns it:

```c
typedef struct kv_pipe {
    kv_gid_t         id;
    kv_pipe_end_t    *read_end;
    kv_pipe_end_t    *write_end;
    ring_buffer_t   *buffer;
    kv_mutex_t       lock;
} kv_pipe_t;

typedef struct kv_pipe_end {
    kv_gid_t         owner_pid;     // (node_id, pid) — for now node_id==self
    kv_pipe_t       *pipe;
    bool             closed;
    wait_queue_t     waiters;
} kv_pipe_end_t;
```

**The fabric win:** A pipe with one endpoint local and one remote becomes "send the bytes over fabric" — but the rest of pipe semantics (blocking read, EPIPE on closed write end, etc.) doesn't change because the structure already captures the endpoint identities.

### 6.2 Signals are queueable and serializable

Signals must not be delivered "directly into registers." They must go through a per-process **pending signal queue**, with each signal a serializable record:

```c
typedef struct kv_signal {
    int           signum;
    kv_gid_t      sender;        // who sent it
    uint64_t      timestamp;
    uint32_t      flags;
    union {
        int   sival_int;
        void *sival_ptr;
    } sig_value;
} kv_signal_t;

typedef struct proc {
    // ... existing ...
    struct list_head pending_signals;
    kv_spinlock_t    signal_lock;
} proc_t;
```

**Why:** During checkpoint, you serialize the pending signal queue. After restore, signals are delivered. Without queueing, a signal that arrives at the moment of migration is lost.

### 6.3 Wait queues track wait conditions explicitly

When a process blocks (waiting on a pipe, file, etc.), record *why*:

```c
typedef enum kv_wait_reason {
    KV_WAIT_PIPE_READ,
    KV_WAIT_PIPE_WRITE,
    KV_WAIT_FILE_DATA,
    KV_WAIT_SIGNAL,
    KV_WAIT_CHILD_EXIT,
    KV_WAIT_FABRIC_REPLY,    // future
} kv_wait_reason_t;

typedef struct kv_wait_record {
    kv_wait_reason_t reason;
    kv_gid_t         target;     // pipe id, file id, child pid, etc.
    uint64_t         wait_start_tsc;
} kv_wait_record_t;

typedef struct proc {
    // ... existing ...
    kv_wait_record_t blocked_on;
} proc_t;
```

**The DSSI win:** When checkpointing a blocked process, you serialize its wait record. After restore on another node, you reconstruct the wait — perhaps by registering interest in the same pipe (now via fabric).

### 6.4 fork() returns are migration-friendly

`fork()` is a special case for the fabric: a newly-forked child is a *perfect* candidate for migration to an idle node (it has minimal state — just COW pages with the parent).

Design the fork implementation so the parent can pass a *hint*:

```c
pid_t fork_with_hint(uint32_t hints);
#define FORK_HINT_MIGRATABLE  0x01   // child is a good migration candidate
#define FORK_HINT_CPU_HEAVY   0x02   // child will use lots of CPU
#define FORK_HINT_PIN_LOCAL   0x04   // must stay local
```

For now, hints are advisory and ignored. Later, the fabric scheduler reads them on fork to decide whether to immediately migrate the child to an idle node.

---

## 7. Phase 11 — Networking Seeds

This is the make-or-break phase for DSSI. The networking decisions you make here determine fabric performance forever.

### 7.1 Build TWO network paths, not one

The kernel needs two distinct networking subsystems:

```
USER APPS (sockets API)                       FABRIC LAYER
       │                                              │
       ▼                                              ▼
  ┌─────────────────────┐                  ┌─────────────────────┐
  │  TCP/IP stack       │                  │ Fabric link layer   │
  │  (user-facing)      │                  │ (kernel-internal)   │
  └─────────────────────┘                  └─────────────────────┘
              │                                      │
              └──────────────┬───────────────────────┘
                             ▼
                  ┌─────────────────────┐
                  │  NIC driver         │
                  │  (multi-queue)      │
                  └─────────────────────┘
```

**Why two:** User-facing sockets need full TCP semantics, but they're slow and have a complex API. The fabric layer has *one* peer per remote node and can use a custom, fast, kernel-internal protocol. Mixing them was a mistake every prior SSI made.

**Implementation:** Phase 11 builds the NIC driver and a basic IPv4/UDP stack for users. **Reserve a UDP port range** (e.g., 5000-5099) for fabric use. The fabric protocol (Phase 16+) will use that range with its own header format on top of UDP, bypassing the user TCP/IP stack.

### 7.2 Ring-buffer-based packet I/O

NIC drivers should expose packets via ring buffers, not callbacks or copies. This is the io_uring model and it is the highest-performance pattern available.

```c
typedef struct kv_packet_ring {
    void          *entries;          // array of slot structs
    uint32_t       head;             // producer position
    uint32_t       tail;             // consumer position
    uint32_t       size;             // power of 2
    uint8_t       *buffer_pool;      // packet payloads live here
} kv_packet_ring_t;
```

Even in QEMU with virtio-net, design your driver as if it's talking to a multi-queue 100 GbE NIC. The fabric will need this.

### 7.3 Multi-queue from day one (even if you have one queue)

Real NICs (and virtio-net with multiqueue) expose multiple RX/TX queues, one per CPU. Even if your kernel is single-CPU today (32-bit, no SMP yet), structure the driver as having an *array* of queues — just length 1 for now.

```c
typedef struct kv_nic {
    char            name[16];
    uint32_t        num_rx_queues;       // 1 for now
    uint32_t        num_tx_queues;
    kv_packet_ring_t *rx_rings;          // array
    kv_packet_ring_t *tx_rings;
    // ...
} kv_nic_t;
```

When you add SMP support later, the queue array trivially scales.

### 7.4 Reserve frame types for fabric in the link layer

If you do anything below IP (e.g., custom Ethernet frame types), reserve a custom EtherType for fabric traffic:

```c
#define KV_ETHERTYPE_IP        0x0800
#define KV_ETHERTYPE_ARP       0x0806
#define KV_ETHERTYPE_KV_FABRIC 0x88B5   // experimental ranges
```

Frames with this type are dispatched directly to the fabric layer, skipping IP entirely. Latency win.

### 7.5 Zero-copy receive path

Design the receive path so that the packet payload can be *handed to* the consumer (fabric or user) **without copying**. Reference counting on packet buffers makes this clean:

```c
typedef struct kv_mbuf {
    uint8_t        *data;
    uint32_t        len;
    uint32_t        capacity;
    atomic_t        refs;
    void          (*free)(struct kv_mbuf *);
    struct kv_mbuf *next;       // chain for fragmented payloads
} kv_mbuf_t;

void mbuf_get(kv_mbuf_t *m);     // bump refcount
void mbuf_put(kv_mbuf_t *m);     // drop ref; frees when zero
```

A migration that sends 1 GB of process memory should *never* copy the bytes from user space to a kernel buffer to a NIC buffer. Three copies = 3 GB of memory traffic for a 1 GB transfer. Zero-copy means one DMA descriptor pointing at the page.

### 7.6 Per-packet timing instrumentation

Every packet that the NIC delivers gets a hardware timestamp (if NIC supports it) or a TSC reading. The fabric latency analysis (Phase 17+) needs this:

```c
typedef struct kv_mbuf {
    // ... above fields ...
    uint64_t       rx_timestamp_tsc;
} kv_mbuf_t;
```

### 7.7 Start with UDP + custom reliability, not TCP

TCP is months of work to do correctly (windowing, congestion control, retransmission). For the fabric, you do not need TCP — you need:
- Reliable delivery (ACKs, retransmits)
- Ordering (sequence numbers)
- Flow control (windowing)
- *Low latency* (most important; TCP optimizes for throughput)

Phase 11 builds UDP. Phase 16+ builds a custom reliable transport on top of UDP, inspired by **QUIC** (which gets you the above features with much lower latency than TCP). The user-facing TCP can come later.

**Why this is OK:** The fabric is a controlled environment. You own both endpoints. You don't need TCP's interoperability with the rest of the internet.

---

## 8. The DSSI-Readiness Checklist

Use this checklist at the end of each phase. If you can check every box, you have not built a DSSI debt for later.

### End of Phase 6 (Driver Model)
- [ ] Every `kv_device_t` has a `kv_gid_t id` field
- [ ] Devices have `proxy_class` and `fabric_name` fields
- [ ] All device access goes through the ops table (no direct calls)
- [ ] Per-device stats counters in place

### End of Phase 7 (Block I/O)
- [ ] Block addresses are `(device_gid, lba)` triples, not bare `(dev, lba)`
- [ ] Buffer cache entries record `source`, `last_writer_pid`, `last_access_tsc`
- [ ] `block_submit()` goes through a hookable function pointer
- [ ] Async submission is the primitive; sync is a wrapper
- [ ] Per-process dirty buffer lists

### End of Phase 8 (VFS)
- [ ] Every `kv_file_t` has `gid` and `home_node` fields
- [ ] Every `kv_inode_t` has `gid` and `version` fields
- [ ] Per-process `namespace_t` indirection in place (most processes share one)
- [ ] File ops table includes `is_cacheable` hint
- [ ] Mounted filesystems declare `kv_fs_locality_t`

### End of Phase 9 (Userland)
- [ ] All process memory regions tracked as `kv_vma_t` structs
- [ ] `kv_syscalls.h` defines syscall numbers stably; fabric range 200-255 reserved
- [ ] libc uses single `kv_syscall()` inline; no inline `int 0x80` in user code
- [ ] Shell has stub `fabric` command

### End of Phase 10 (IPC)
- [ ] Pipes have endpoint owner identities
- [ ] Signals are queued (not delivered straight to registers)
- [ ] Wait records include `kv_wait_reason_t` and `kv_wait_record_t`
- [ ] `fork()` accepts (and may ignore) migration hints

### End of Phase 11 (Networking)
- [ ] Fabric link layer is a separate code path from user TCP/IP
- [ ] NIC drivers expose ring buffers (even if length 1)
- [ ] Multi-queue architecture in place (even if one queue exists)
- [ ] Custom EtherType reserved for fabric
- [ ] Zero-copy mbuf with refcounts
- [ ] UDP works; fabric port range (5000-5099) reserved

### Cross-phase, ongoing
- [ ] All locks classified as spinlock vs mutex; lock-discipline doc maintained
- [ ] `KV_TRACE` macro defined and sprinkled through all subsystems
- [ ] Every kernel object has `serialize` / `deserialize` stubs
- [ ] Every metric designed to aggregate across nodes

---

## 9. What to Resist Adding

Equally important: things you might be tempted to build now that you **should not**, because they will be different (or unnecessary) when fabric arrives.

### 9.1 Do NOT build a "remote procedure call" library yet

It's tempting in Phase 11 to build a generic RPC system "for the fabric to use later." Don't. The fabric protocol (Phase 17+) is highly specialized. A general RPC library will be the wrong shape. Build it when you need it.

### 9.2 Do NOT add SMP (multi-CPU) support yet

SMP support is its own multi-month project (per-CPU run queues, RCU, fine-grained locking). DSSI does not need SMP. Stay single-CPU per node through Phase 22 at least. Each fabric node is a single-CPU kernel; the fabric provides "multi-node" parallelism without "multi-CPU" parallelism.

### 9.3 Do NOT prematurely encrypt fabric communication

Phase 11's networking should not include TLS or encryption. The fabric layer (Phase 16+) will eventually add encryption (Phase 27), but the early control-plane prototypes are easier to debug in plaintext (Wireshark, tcpdump). Encryption is a *late* addition.

### 9.4 Do NOT build user-facing fabric APIs yet

Don't add `fabric_migrate()` syscalls in Phase 9. The shell stub from Seed 5.5 is enough. Real syscalls land in Phase 18+ when there's a fabric for them to talk to.

### 9.5 Do NOT use the `unsigned long` / `long` types

These are 32 bits on 32-bit x86 and 64 bits on 64-bit x86 — they will break your serialization when you do the 64-bit port. Use explicit `uint32_t`, `uint64_t`, `int32_t`, `int64_t` everywhere in serializable structures. Use `uintptr_t` only for pointers.

### 9.6 Do NOT skip writing tests

Every seed in this document is testable. Write the tests as you go. The fabric work in Phase 16+ will mercilessly find every latent assumption violated by the foundation; tests catch them early.

---

## Closing Note

The seeds in this document are deliberately *small*. None of them require new subsystems. None of them slow down your Phase 6-11 work meaningfully. Most are just "add this one field," "rename this struct," "put this code behind an indirection."

But together, they are the difference between **adding the fabric layer in Phase 16 as a clean module** versus **rewriting half the kernel from scratch when you realize what fabric needs**.

The single most important seed is **Seed 1.1: every kernel object has a `kv_gid_t`**. If you only do one thing from this document, do that. Everything else follows from it.

---

**Next document to write:** `dssi_02_observability.md` — TraceOS and the resource truth ledger, from the fabric layer's perspective. This describes what data the fabric needs from the local kernel to make good decisions, and how TraceOS becomes the fabric's "shared memory" for causal debugging across nodes.
