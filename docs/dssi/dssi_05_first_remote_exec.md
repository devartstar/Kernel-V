# DSSI 05 — First Remote Execution (Phases 18-19)

> **Scope:** Phases 18 and 19. *This is the first document where the user's original vision actually runs.* A process starts on node A. It pauses. It moves to node B. It resumes on node B. It opens a file — and the read syscall transparently reaches back to node A to fetch the data, because the file lives on A's disk. The process never knows it moved.
>
> **Position in the master plan:** Part 5 of 8. We have a fabric (Doc 04), checkpoints (Doc 03), observability (Doc 02), and DSSI-shaped kernel objects (Doc 01). Now we wire them together into the simplest possible remote execution primitive — *stop-and-copy migration* — and then add the *deputy* mechanism that lets a migrated process keep using resources that stayed home. Phase 22 will make migration live (no pause). Phase 20 will make scheduling smart. For now: simplest possible thing that works end-to-end.
>
> **Why this is the most exciting document so far:** every other phase has been infrastructure. Phase 18 is when you SSH into node-a, run `./long_running_thing &`, type `migrate <pid> node-b`, and watch the process continue running on a different machine. The vision becomes touchable.

---

## 0. Thinking Process — Why Stop-and-Copy First, Why Syscall Proxy Second

### Why "stop-and-copy" before "live" migration

There are four kinds of process migration, in order of increasing sophistication:

| Style | How it works | Pause time | Implementation cost |
|-------|--------------|------------|---------------------|
| **Stop-and-copy** | Freeze → checkpoint → ship → restore → resume | Seconds (proportional to memory size) | Low |
| **Pre-copy** | Iteratively ship dirty pages while still running, then brief stop | <100ms typical | Medium |
| **Post-copy** | Brief stop → ship registers/metadata → resume on target → fault pages on demand | <50ms typical | High |
| **Hybrid** | Combine pre-copy + post-copy, switching based on dirty rate | <30ms typical | Very high |

Why start with stop-and-copy when it has the worst pause time?

**Because it's the only one where every piece of state arrives before the process resumes.** That means:
- No race conditions between "process is using page X" and "page X is still being shipped"
- No demand-fault network round-trips
- No need to maintain a synchronization channel between source and destination during execution
- Test failures point at *one* of: checkpoint correctness, transport correctness, restore correctness — never all three interleaved

It's also how every real production migration system was built. CRIU started with stop-and-copy and only got pre-copy in 2014, *years* after launch. QEMU live migration's first published version was stop-and-copy. VMware's vMotion announcement paper (2005) reads like an academic justification for why pre-copy was worth the complexity *after* stop-and-copy was already industrial.

We will follow the same path. **Phase 18 ships stop-and-copy. Phases 20-22 evolve toward live.**

### Why syscall proxying comes immediately after migration

The naive vision is: "process moves to new node, runs there, done." Reality bites within 100ms of the first demo:

```c
// Process on node-a opens /var/log/app.log, writes a line every second
fd = open("/var/log/app.log", O_WRONLY|O_APPEND);
while (1) {
    write(fd, "tick\n", 5);   // <-- migrate happens here
    sleep(1);
}
```

After migration to node-b, what does `write(fd, ...)` do? The file `/var/log/app.log` is on **node-a's** disk. node-b has its own `/var/log/app.log` (different content, possibly nonexistent). The process expects to keep writing to the same file it opened on node-a.

Three choices:

1. **Pre-migration: copy all open resources.** Doesn't work for sockets (peer expects packets from node-a's IP), shared memory (other processes still on node-a use it), or large files (would have to copy gigabytes).

2. **Disallow migration if process has open resources.** Almost no useful program is migratable.

3. **Keep resources on home node, forward syscalls back via fabric.** This is the **MOSIX/openSSI/Sprite "deputy" model**, and it's the only one that scales. Phase 19 builds it.

So: Phase 18 makes migration *exist*, Phase 19 makes it *useful*. Splitting them keeps each phase debuggable.

### The home-node concept — this is the load-bearing abstraction

Every process in the fabric has exactly one **home node**: the node where it was originally `fork()`ed or `exec()`d. The home node is the canonical owner of:
- The process's PID (PIDs are globally unique because they're `kv_gid_t` — node_id + local_id from Doc 01)
- The process's file descriptor table
- The process's open files, sockets, pipes
- The process's parent-child relationships in the global process tree
- The process's signal mailbox

When a process migrates from home node A to **execution node** B, what travels is the *execution context*: registers, stack, heap, anonymous mappings, process state. What stays home is everything that has external identity (FDs pointing to files, sockets bound to ports, etc.).

This is fundamentally different from container migration (which copies the world) or VM migration (which copies the machine). It's a *process-level* migration that intentionally splits "what runs" from "what's identifiable."

The deputizing pattern means every syscall on the execution node first asks: "does this touch any resource owned by home?" If yes → forward to home, wait for result. If no → execute locally.

We will pay a latency cost for forwarded syscalls (typically 100-500μs on a LAN). This is the right tradeoff *until* Phase 23, when we add the distributed scheduler that decides whether to migrate the resource itself.

---

## 1. The Deliverable — What Works at the End of Phase 19

After Phase 19 you can run this on a 2-node fabric:

```
(node-a console)
kv> ps
  PID         CMD          NODE     CPU%   MEM      STATE
  100         init         a:LOCAL  0      4MB      S
  142         shell        a:LOCAL  0      8MB      R
  201         counter      a:LOCAL  87     2MB      R

kv> cat /proc/201/status
  PID:        a:201            (global: node_a:201)
  Home:       node-a (this node)
  Exec:       node-a (this node)
  State:      RUNNING
  CPU%:       87
  Memory:     2 MB

kv> migrate 201 node-b
[FABRIC] target candidate: node-b (cpu=12%, mem_free=240MB, phi=0.4) -> OK
[KV_TRACE] MIGRATE_START pid=a:201 target=node-b
[MIGRATE] phase=freeze        elapsed=2ms
[MIGRATE] phase=checkpoint    elapsed=145ms   size=2.1MB
[MIGRATE] phase=transport     elapsed=87ms    throughput=24MB/s
[MIGRATE] phase=restore       elapsed=110ms
[MIGRATE] phase=handoff       elapsed=4ms
[KV_TRACE] MIGRATE_COMPLETE pid=a:201 elapsed=348ms
[FABRIC] process a:201 now executes on node-b, home remains node-a

kv> ps
  PID         CMD          NODE              CPU%   MEM      STATE
  100         init         a:LOCAL           0      4MB      S
  142         shell        a:LOCAL           0      8MB      R
  201         counter      a:HOME b:EXEC     85     2MB      R

kv> cat /proc/201/status
  PID:        a:201            (global: node_a:201)
  Home:       node-a (this node)
  Exec:       node-b
  State:      RUNNING (remote)
  CPU%:       85
  Memory:     2 MB
  Syscalls proxied (last 1s): 1 (write)
  Syscalls local (last 1s):   0
  Avg proxy latency:          180μs

(on node-b)
kv> ps
  PID         CMD          NODE              CPU%   MEM      STATE
  100         init         b:LOCAL           0      4MB      S
  201         counter      a:HOME b:EXEC     85     2MB      R   <-- the migrated process

(back on node-a, the original log file is still being written by the proxied write() syscalls)
kv> tail -f /var/log/counter.log
  count=1
  count=2
  count=3
  ... (continues uninterrupted, even though the process is running on node-b)
```

The user typed `migrate 201 node-b`. Three hundred milliseconds later, the process is running on a different machine, still writing to the original log file. **That's the vision, working.**

---

## 2. Phase 18 — Stop-and-Copy Migration

### 2.1 Goals

1. A privileged command (`migrate <pid> <target_node>`) initiates migration.
2. The source node freezes the process (reuses Doc 03 freeze protocol).
3. The source node serializes the process into a checkpoint image (reuses Doc 03 chunks).
4. The image streams to the target node over the fabric link (Doc 04).
5. The target node restores the image into a new process (reuses Doc 03 restore).
6. The source node forwards the global PID to point at the target.
7. The target node thaws the process; the source node tombstones it.
8. End-to-end time for a 2MB process: <500ms on a 1Gbps virtual network.

What's explicitly **out of scope** for Phase 18:
- Open files, sockets, pipes — Phase 18 *refuses* to migrate a process with open FDs (Phase 19 lifts this restriction with deputizing)
- Pre-copy or any form of overlapping execution — Phase 22
- Automatic target selection — operator picks the target
- Migration of multi-threaded processes — Phase 18 supports single-threaded only

These constraints make Phase 18 testable in isolation.

### 2.2 The Wire Protocol

Five fabric message types, added to the `KV_FMSG_*` set:

| Message | Direction | Payload | Purpose |
|---------|-----------|---------|---------|
| `KV_FMSG_MIGRATE_REQUEST` | source → target | global PID, process metadata digest, image size | "Can you accept this process?" |
| `KV_FMSG_MIGRATE_ACCEPT` | target → source | acceptance flag, reservation token | "Yes, I've reserved space" |
| `KV_FMSG_MIGRATE_REJECT` | target → source | reason code | "No, here's why" |
| `KV_FMSG_MIGRATE_CHUNK` | source → target | sequence, chunk_id, payload bytes | One chunk of the checkpoint image |
| `KV_FMSG_MIGRATE_COMPLETE` | source → target | total size, CRC32 of image | "That was the last chunk" |
| `KV_FMSG_MIGRATE_RESTORED` | target → source | success/failure, new exec_node_id | "Process is alive on me" or "Failed, here's why" |

### 2.3 The Full Migration Sequence

```
       SOURCE (home)                          TARGET (will be exec)
         |                                        |
         |  [operator: migrate 201 node-b]        |
         |                                        |
         |  1. fabric_resource_check(target)      |
         |     - target.phi < 2.0 ?               |
         |     - target.state == ALIVE ?          |
         |     - target.mem_free >= proc.size*1.5 |
         |     - target.cpu_util < 80% ?          |
         |  PASS                                  |
         |                                        |
         |  2. proc_pre_migration_checks(pid)     |
         |     - proc.state == RUNNING            |
         |     - proc.open_fd_count == 0 (Ph 18)  |
         |     - proc.thread_count == 1 (Ph 18)   |
         |     - proc.exec_node == HOME           |
         |  PASS                                  |
         |                                        |
         |  3. send MIGRATE_REQUEST -------------->|
         |                                        |
         |                                        |  4. target validates:
         |                                        |     - has resources?
         |                                        |     - is image_size safe?
         |                                        |     - reserve mem
         |                                        |
         |<-- 5. MIGRATE_ACCEPT -------------------|
         |                                        |
         |  6. freeze process                     |
         |     - mark state = PROC_FROZEN         |
         |     - wait for syscalls to drain       |
         |     - flush pending signals to mailbox |
         |                                        |
         |  7. serialize to checkpoint image      |
         |     (Doc 03 chunked format)            |
         |                                        |
         |  8. stream chunks ------------------->>>|
         |     MIGRATE_CHUNK × N                  |
         |                                        |
         |                                        |  9. target accumulates,
         |                                        |     verifies sequence,
         |                                        |     deserializes incrementally
         |                                        |     into a frozen process shell
         |                                        |
         |  10. send MIGRATE_COMPLETE ----------->|
         |      (with image CRC)                  |
         |                                        |
         |                                        | 11. validate CRC
         |                                        |     finish restore
         |                                        |     assign exec_node = self
         |                                        |     do NOT thaw yet
         |                                        |
         |<-- 12. MIGRATE_RESTORED ---------------|
         |        (with target.node_id)           |
         |                                        |
         |  13. atomic switchover:                |
         |      - update global PID table:        |
         |        a:201 exec_node = node_b        |
         |      - announce via gossip             |
         |        (priority broadcast)            |
         |                                        |
         |  14. send MIGRATE_THAW -------------->>|
         |                                        |
         |                                        | 15. target thaws process
         |                                        |     - state = RUNNING
         |                                        |     - schedule on local CPU
         |                                        |
         |  16. tombstone local frozen copy       |
         |      - keep PID entry as "exec=remote" |
         |      - free local pages                |
         |      - close local kernel structures   |
         |                                        |
         |                                        | 17. process runs on target
```

Every step emits a `KV_TRACE` event. Every step is undoable up through step 12 (if anything fails, source thaws original and target releases its reservation).

### 2.4 The Atomic Switchover — The Hardest Step

Step 13 is where bugs hide. It must be atomic with respect to:

- Other migrations of the same process (impossible by construction; we lock per-PID)
- `kill` / signal delivery to the process (signals are routed via home node; home node redirects them to target during/after switchover)
- `wait()` from the parent (parent reads from global PID table; sees old or new location atomically)
- `ps` / proc reads (read via home node's per-PID lock)

The implementation uses a **per-PID seqlock** on the global PID table entry:

```c
typedef struct kv_global_pid_entry {
    kv_gid_t        pid;                 // global PID
    kv_gid_t        home_node;
    kv_gid_t        exec_node;           // may differ from home_node
    seqlock_t       location_lock;       // read-mostly
    uint32_t        location_version;
    // ... other fields
} kv_global_pid_entry_t;
```

Updates to `exec_node` increment `location_version`. Readers retry if version changes during their read. This is the same pattern Linux uses for `task_struct->state`.

### 2.5 The "What Travels, What Stays" Inventory

Critical to get exactly right. From the checkpoint image format in Doc 03, here's what's in each chunk and what we do with it:

| Chunk | Phase 18 behavior |
|-------|-------------------|
| `HEADER` | Travels; receiver validates |
| `PROCESS_META` (uid, gid, cwd, args, env) | Travels; restored verbatim |
| `REGISTERS` | Travels; loaded into new process |
| `KERNEL_STACK` | Travels; restored as new kernel stack |
| `VMA_LIST` (memory map layout) | Travels; layout reconstructed |
| `PAGES` (anonymous page contents) | Travels; copied into new physical pages |
| `FD_TABLE` | **Phase 18: must be empty (rejected otherwise)** |
| `OPEN_FILES` | **Phase 18: must be empty** |
| `PENDING_SIGNALS` | Travels; queued on target |
| `WAIT_STATE` | Travels; reconstructed |
| `NAMESPACE` | Travels (Phase 19 modifies this) |
| `TRAILER` | CRC validation |

The FD/OPEN_FILES restriction is enforced by Phase 18 and lifted by Phase 19 (which adds remote FD references).

### 2.6 Failure Modes and Rollback

| Failure point | Detection | Recovery |
|---------------|-----------|----------|
| Pre-check fails (target busy, dead, low mem) | Local | Don't start; report error |
| Target rejects MIGRATE_REQUEST | Sync response | Don't start; try another target |
| Network drops during chunk stream | TCP error / phi spike | Abort, thaw original |
| Target CRC mismatch | MIGRATE_RESTORED carries failure | Abort, thaw original, log corrupt-transport |
| Target fails to restore (OOM during page copy) | MIGRATE_RESTORED failure | Abort, thaw original |
| Target dies mid-handoff (between RESTORED and THAW) | Phi rises on target | If we got RESTORED, attempt re-migration to another target using local frozen copy |
| Target dies after THAW | Phi rises, exec_node unreachable | Process is **gone** (Phase 18 limitation; Phase 21 adds checkpoint replicas) |

The "process is gone" failure is real and is a known Phase 18 limitation. Documented as acceptable for now; addressed in Phase 21 with "shadow checkpoints" kept on a second peer.

### 2.7 Performance Targets for Phase 18

| Process size | Target migration time (LAN, 1Gbps) |
|--------------|-------------------------------------|
| 1 MB | <200ms |
| 10 MB | <500ms |
| 100 MB | <3s |
| 1 GB | <30s |

Bottleneck analysis at 1Gbps:
- Theoretical peak transport: 125 MB/s
- After protocol overhead + frame headers: ~100 MB/s
- 100MB process: 1s in transport alone + ~200ms restore + ~50ms freeze = ~1.3s, leaving headroom against the 3s target

These targets are deliberately loose so Phase 18 isn't optimization-blocked. Phase 22 will halve them via pre-copy.

### 2.8 Phase 18 Test Strategy

1. **Trivial migration:** boot 2 nodes, run a busy-loop on A, migrate to B, watch CPU% on B rise.
2. **Counting test:** process increments a counter in anonymous memory every 100ms and writes to stdout (which goes to console = no FD test). Migrate mid-run. Verify count continues without gap or duplication.
3. **State preservation test:** process allocates 50MB, fills with known pattern. Migrate. Verify all 50MB unchanged on target (CRC).
4. **Signal delivery test:** parent sends SIGUSR1 to child immediately after `migrate` issued. Verify SIGUSR1 arrives after thaw, exactly once.
5. **Rollback test:** induce target failure between MIGRATE_ACCEPT and MIGRATE_RESTORED (test hook). Verify process resumes on source intact.
6. **Performance test:** measure all four points in §2.7 table; track in CI dashboard.
7. **Concurrent migration test:** migrate two different processes simultaneously to different targets. Verify both succeed.
8. **Stress test:** migrate-pong: A→B→A→B→A 100 times, check process state preserved throughout.

### 2.9 Phase 18 Implementation Phasing (5 weeks)

| Week | Deliverable | Verifiable result |
|------|-------------|-------------------|
| 1 | Global PID table, location seqlock, `migrate` shell command stub | `migrate` command parses; PID table queryable |
| 2 | Migration protocol messages, source-side freeze/serialize/stream | Source can prepare and send a complete image; target logs receipt |
| 3 | Target-side receive/deserialize/restore (NOT thaw) | Target reconstructs frozen process; verifies CRC; reports RESTORED |
| 4 | Atomic switchover, thaw, source tombstone | First end-to-end migration of a busy-loop process works |
| 5 | Failure handling, rollback paths, all 8 tests passing, polish | Performance targets met; all tests green |

---

## 3. Phase 19 — Syscall Proxy & Deputizing

### 3.1 Goals

1. A process with open file descriptors can be migrated.
2. After migration, syscalls touching those FDs are transparently forwarded to the home node.
3. The forwarding is invisible to the userspace process — same syscall numbers, same return values, same errno semantics.
4. Overhead per forwarded syscall: <500μs on LAN.
5. Local syscalls (no home-node resource) execute fully on the exec node with zero forwarding overhead.
6. Forwarding is observable via TraceOS so an operator can see "this process is doing 80% of its syscalls remotely, maybe we should migrate the resource."

### 3.2 The Deputy Concept — One Page Summary

Imagine a process opens 5 files on node A, then migrates to node B.

On node A, we keep a **deputy thread** that exists solely to execute syscalls on behalf of the migrated process. The deputy holds:
- The process's FD table (the *real* one)
- The process's open `struct kv_file` references
- The process's namespace (mounts, cwd, etc.)
- A small inbox for incoming syscall forwards

When the process on B issues `write(fd=3, buf, len)`:
1. Exec node's syscall handler checks: is FD 3 a `KV_FD_REMOTE`?
2. Yes → serialize syscall request (syscall_nr, args, buf_data if needed)
3. Send `KV_FMSG_SYSCALL_REQUEST` to home node
4. Home node's deputy thread receives it
5. Deputy executes the syscall on the *real* FD using the local kernel
6. Deputy serializes the result (return value, errno, output buffer if any)
7. Send `KV_FMSG_SYSCALL_RESPONSE` back to exec node
8. Exec node's syscall handler unblocks the process and returns the result

To userspace, the syscall looks identical. Slower (typically 150-400μs on a LAN), but correct.

### 3.3 The Two New FD Types

Phase 19 extends the file descriptor abstraction:

```c
typedef enum kv_fd_type {
    KV_FD_LOCAL = 0,       // open file/socket/pipe on this node
    KV_FD_REMOTE = 1,      // proxy reference to FD on home node
    KV_FD_HOME_OWNED = 2,  // FD this node owns on behalf of an exec'd-away process
} kv_fd_type_t;

typedef struct kv_fd_entry {
    kv_fd_type_t type;
    union {
        struct kv_file *local_file;        // KV_FD_LOCAL
        struct {                           // KV_FD_REMOTE
            kv_gid_t home_node_gid;
            uint64_t home_fd_token;        // opaque handle from home
            // cached metadata for fstat() short-circuit
            uint64_t cached_size;
            uint64_t cached_mtime_ms;
        } remote;
        struct {                           // KV_FD_HOME_OWNED
            struct kv_file *local_file;
            kv_gid_t exec_node_gid;
            // accounting: how many remote syscalls referenced this FD
            uint64_t remote_call_count;
            uint64_t remote_byte_count;
        } home_owned;
    };
} kv_fd_entry_t;
```

When a process migrates with open FDs:
- On the source (home) node, each `KV_FD_LOCAL` becomes `KV_FD_HOME_OWNED` and gets a `home_fd_token` (a 64-bit handle unique to this node).
- On the target (exec) node, the FD table has `KV_FD_REMOTE` entries pointing back at home with those tokens.

### 3.4 The Syscall Classification

Every syscall in Kernel-V is classified into one of three categories:

| Category | Examples | Behavior on exec node |
|----------|----------|------------------------|
| **Pure local** | `getpid`, `getuid`, `time`, `nanosleep`, `mmap (anonymous)`, `brk`, `clock_gettime` | Execute locally |
| **FD-touching** | `read`, `write`, `lseek`, `fstat`, `close`, `ioctl`, `mmap (file-backed)` | Check FD type; forward if `KV_FD_REMOTE` |
| **Inherently global** | `fork`, `wait`, `kill`, `getppid`, `setsid`, signal-related | Forward to home unconditionally |

The classification is done by a per-syscall flag set at compile time. The syscall dispatcher looks up the flags and decides routing in one branch:

```c
long do_syscall(uint32_t nr, uint64_t args[6]) {
    if (current->exec_node_is_home) {
        // No migration in effect; execute locally
        return syscall_table[nr](args);
    }

    uint32_t flags = syscall_flags[nr];

    if (flags & SF_INHERENTLY_GLOBAL) {
        return forward_syscall_to_home(nr, args);
    }

    if (flags & SF_FD_TOUCHING) {
        int fd = args[0];  // by convention, FD is arg0 for fd-touching syscalls
        if (fd_is_remote(current, fd)) {
            return forward_syscall_to_home_with_fd_translation(nr, args, fd);
        }
    }

    // Pure local, or FD-touching with local FD only
    return syscall_table[nr](args);
}
```

One branch per call. Hot path is fast.

### 3.5 Forwarding a Syscall — The Wire Format

```c
typedef struct kv_syscall_request {
    uint64_t  request_id;        // for matching response
    kv_gid_t  pid;
    uint32_t  syscall_nr;
    uint64_t  args[6];
    // FD translation: replace local FD numbers in args with home_fd_tokens
    uint32_t  inline_buf_len;
    uint8_t   inline_buf[];      // for write()-style outbound data, up to 4KB
} kv_syscall_request_t;

typedef struct kv_syscall_response {
    uint64_t  request_id;
    int64_t   return_value;
    int32_t   errno_val;
    uint32_t  inline_buf_len;
    uint8_t   inline_buf[];      // for read()-style inbound data, up to 4KB
} kv_syscall_response_t;
```

For payloads >4KB (large `read`/`write`), the request includes a "use streaming" flag and a separate streaming channel handles the bulk transfer. Most syscalls fit comfortably inline.

### 3.6 The Deputy Thread on the Home Node

When process P with home=A migrates to exec=B, node A allocates:

```c
typedef struct kv_deputy {
    kv_gid_t                proc_pid;
    struct kv_proc         *home_shadow_proc;  // the original task_struct (frozen for state)
    struct kv_msg_queue     inbox;
    struct list_head        pending_requests;
    kv_gid_t                exec_node;
    struct kthread         *deputy_thread;
    uint64_t                requests_handled;
    uint64_t                bytes_proxied_in;
    uint64_t                bytes_proxied_out;
} kv_deputy_t;

void deputy_main(kv_deputy_t *d) {
    while (!d->shutdown_requested) {
        kv_syscall_request_t *req = kv_msg_queue_recv(&d->inbox);
        kv_syscall_response_t resp;

        // Translate home_fd_tokens back to local FD numbers
        translate_fd_args(d, req);

        // Execute the syscall in the context of the shadow process
        kv_with_proc_context(d->home_shadow_proc, {
            resp.return_value = syscall_table[req->syscall_nr](req->args);
            resp.errno_val = (resp.return_value < 0) ? -resp.return_value : 0;
            // gather output buffer if applicable
            collect_output_buf(&resp, req);
        });

        resp.request_id = req->request_id;
        fabric_send(d->exec_node, KV_FMSG_SYSCALL_RESPONSE, &resp, sizeof(resp));
        d->requests_handled++;
    }
}
```

One deputy thread per remotely-executing process. Cost: ~16KB stack each. For 1000 migrated processes, 16MB total. Acceptable.

### 3.7 Why This Is Better Than the Alternatives

Alternative A: ship file contents at migration time.
- Costs gigabytes for large files; defeats the point of migration
- Doesn't work for special files (sockets, /dev/, pipes, /proc/)
- Coherence nightmare if another process on home still has the file open

Alternative B: distributed filesystem (NFS-style) for everything.
- Massive infrastructure project (it's literally Phase 24)
- Slower for the common "files small, syscalls many" case
- Doesn't help with non-file resources (sockets, pipes, signals)

Alternative C: deputizing (this approach).
- Zero copy of file content for the migration itself
- File reads only cross the network when they would have hit the home disk anyway
- Generalizes to *all* resources, not just files
- Simple model: "if it has external identity, it stays home"

The cost is per-syscall latency for proxied syscalls. We pay 200μs instead of 1μs for a `write` to a remote FD. For most workloads this is fine; for FD-heavy workloads (a database server), you wouldn't migrate them in the first place, *or* you'd later migrate the resources themselves (Phase 24).

### 3.8 The "Migrate Back to Home" Optimization

Special case: a process can migrate *back* to its home node. When exec=home, no proxying is needed; all FDs become `KV_FD_LOCAL` again. This is the fast path for "this experiment is over, bring it back home."

`migrate <pid> home` is a first-class shell command. It's the cheapest possible migration because the FD table doesn't need to be re-translated; the home_fd_tokens map 1:1 back to local FDs.

### 3.9 Observability — Crucial for Phase 19

Every forwarded syscall emits two trace events: one on the exec node (SYSCALL_FORWARD_SENT), one on the home node deputy (SYSCALL_FORWARD_EXECUTED). The trace_id propagation from Doc 02 stitches them into a single causal chain.

New `/proc/<pid>/status` fields:
```
Syscalls proxied (total):  4592
Syscalls local (total):    18230
Avg proxy latency:         180μs (p50), 410μs (p99)
Bytes proxied in:          2.4MB
Bytes proxied out:         8.1MB
Hottest proxied syscall:   write (3441 calls, 75%)
```

New `doctor` query:
```
kv> doctor explain "why is process 201 slow on node-b?"
[ANALYSIS] process a:201 (exec on node-b)
  - 87% of syscalls are PROXIED to home (node-a)
  - Avg proxy latency: 410μs vs local syscall 1.2μs (340× slower)
  - Hottest proxied: write(fd=3 -> /var/log/counter.log)
  - Recommendation: migrate the file (Phase 24) or migrate process back home
```

This makes the deputy mechanism's tradeoffs visible to humans, which is essential for tuning.

### 3.10 Edge Cases We Must Get Right

| Case | Behavior |
|------|----------|
| Home node dies while proxied syscall in flight | Exec node returns `EHOSTDOWN`; process can handle or die |
| Process forks on exec node | Child created on exec node, home=exec node's id (new family tree branch) |
| Process opens new file on exec node | New FD is `KV_FD_LOCAL` on exec node, *not* proxied home |
| Process closes a remote FD | `KV_FMSG_FD_CLOSE` sent to home; home closes real FD; exec node clears slot |
| Process exits while away | Exec node sends `KV_FMSG_PROC_EXIT` to home; home does final cleanup, notifies parent |
| Home receives signal for a remotely-executing process | Home forwards signal via `KV_FMSG_SIGNAL_DELIVER` to exec node; exec node delivers |
| Concurrent close from both sides | Token-based; whoever sees the token last loses the race cleanly |

Each case has a test. Each test runs in CI.

### 3.11 Phase 19 Test Strategy

1. **Open-then-migrate test:** open `/tmp/foo`, write "hello", migrate, write "world", migrate back, close. Verify file contains "helloworld".
2. **Read-then-migrate test:** open a file, read 1KB on home, migrate, read next 1KB on exec, verify continuity and offset preservation.
3. **Latency budget test:** with a process doing 1000 proxied `write()`s/sec, measure p99 latency. Must be <500μs.
4. **Mixed local/proxy test:** process opens FD on home (becomes remote after migrate), then opens new FD on exec (stays local). Verify the two FDs are routed differently.
5. **Home death test:** kill home node mid-migration; verify exec node detects, exits gracefully.
6. **Signal delivery test:** parent on third node sends SIGUSR1 to a migrated process; verify it arrives.
7. **Fork-on-exec test:** migrated process forks; verify child is born on exec node, parent-child relationship in global tree intact.
8. **Close test:** migrated process closes a remote FD; verify home releases the underlying file; verify exec slot freed.
9. **migrate-back optimization test:** measure migrate-back vs migrate-fresh; back should be measurably faster.

### 3.12 Phase 19 Implementation Phasing (6 weeks)

| Week | Deliverable | Verifiable result |
|------|-------------|-------------------|
| 1 | FD entry type extension; FD translation during migration | Migrate process with open FD; FDs marked REMOTE on exec, HOME_OWNED on home |
| 2 | Syscall classification flags; dispatcher routing | Pure-local syscalls execute locally; FD-touching detected |
| 3 | Forward request/response wire protocol; basic `write` forwarding | Counter-with-logfile demo from §1 works for write |
| 4 | Deputy thread; full FD-touching syscall coverage (`read`, `lseek`, `fstat`, `close`, `ioctl`) | All FD syscalls work transparently across migration |
| 5 | Inherently-global syscalls (signal, fork, wait, kill); edge cases | All 9 tests in §3.11 pass |
| 6 | Observability integration, /proc fields, doctor queries, polish | Operator can explain proxy overhead from shell |

---

## 4. The `KV_TRACE` Events Added in These Phases

```c
// Category: MIGRATE (0x50)
#define KV_TRACE_MIGRATE_START             0x5001
#define KV_TRACE_MIGRATE_FREEZE_DONE       0x5002
#define KV_TRACE_MIGRATE_CHECKPOINT_DONE   0x5003
#define KV_TRACE_MIGRATE_TRANSPORT_START   0x5004
#define KV_TRACE_MIGRATE_TRANSPORT_DONE    0x5005
#define KV_TRACE_MIGRATE_RESTORE_START     0x5006
#define KV_TRACE_MIGRATE_RESTORE_DONE      0x5007
#define KV_TRACE_MIGRATE_HANDOFF           0x5008
#define KV_TRACE_MIGRATE_THAW              0x5009
#define KV_TRACE_MIGRATE_ROLLBACK          0x500A
#define KV_TRACE_MIGRATE_COMPLETE          0x500B

// Category: PROXY (0x60)
#define KV_TRACE_PROXY_SYSCALL_FORWARD     0x6001
#define KV_TRACE_PROXY_SYSCALL_RESPONSE    0x6002
#define KV_TRACE_PROXY_FD_TRANSLATE        0x6003
#define KV_TRACE_PROXY_SIGNAL_FORWARD      0x6004
#define KV_TRACE_DEPUTY_SPAWN              0x6010
#define KV_TRACE_DEPUTY_REAP               0x6011
```

The Kernel Doctor learns to explain proxy chains, migration timelines, and rollback reasons using these events.

---

## 5. How These Phases Set Up Phase 20 (Distributed Scheduler)

At the end of Phase 19 we can migrate manually. Phase 20 will decide *automatically* when and where to migrate. It will need:

1. **A cost model:** every process has metrics (CPU%, memory footprint, syscall rate, proxy fraction). Phase 19 publishes the syscall rate and proxy fraction via TraceOS; the scheduler reads them.
2. **A benefit model:** the Resource Ledger (Doc 02 + Doc 04) tells the scheduler which nodes are underloaded.
3. **A migration primitive:** `migrate <pid> <node>` already exists.
4. **A back-pressure mechanism:** if proxy traffic is high (process is constantly reaching back to home), the scheduler should consider migrating back, *not* further away.

Phase 20 will be ~80% policy code and ~20% glue, because Phases 18-19 built the mechanism.

---

## 6. Risks & Mitigations

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| FD translation has off-by-one or table-corruption bugs | High (complex) | Extensive table-stress tests; FD numbers are 32-bit so plenty of room |
| Signal delivery during migration loses signals | Medium | Pending-signal serialization explicitly in checkpoint; replay on thaw |
| Deputy thread blocks on slow home-side syscall, stalls process | Medium | Deputy uses async I/O where possible (Phase 11 io_uring-like API); per-process deputy means no head-of-line blocking |
| Migration of large process takes minutes, frustrating operator | Low (Phase 18) | Performance targets in §2.7; UX shows progress bar; Phase 22 cuts it 5-10× |
| Process expects sub-microsecond syscall latency, breaks after migrate | Medium | Documented; `migrate` command requires confirmation if process has high syscall rate |
| Home node death loses migrated process | High (Phase 18-19 limitation) | Documented; Phase 21 adds shadow checkpoints on a second peer |
| Inter-node clock skew confuses migration timestamps | Medium | TSC-only on each side; wall-clock display only |
| Privileged migrate command misused (denial-of-service via migrate-storm) | Low (Phase 19) | Rate-limit per-user; auditable via TraceOS; security framework Phase 27 |

---

## 7. Aggregate Timeline & Position in the Roadmap

| Phase | Weeks | Cumulative weeks (this doc) |
|-------|-------|-----------------------------|
| Phase 18: Stop-and-Copy Migration | 5 | 5 |
| Phase 19: Syscall Proxy & Deputizing | 6 | 11 |

Combined with prior docs:
- Phases 6-11 (Doc 01): UNIX foundation
- Phases 12-13 (Doc 02): TraceOS + Resource Ledger — 9 weeks
- Phases 14-15 (Doc 03): Slab + Checkpoint/Restore — 13 weeks
- Phases 16-17 (Doc 04): Fabric Control Plane — 9 weeks
- **Phases 18-19 (Doc 05): First Remote Execution — 11 weeks** ← we are here

At the end of Phase 19, **the user's original vision works in its simplest form**. The remaining four documents make it fast (Doc 06), make it smart (Doc 07), and make it scale to real-world workloads (Doc 07).

---

## 8. The Five Most Important Decisions in This Document

1. **Stop-and-copy first.** Pre-copy and post-copy are evolutions; ship the simplest working migration before optimizing.
2. **Home node is forever.** A process has one home for its lifetime. Migrating "moves the executor, not the identity." This is the load-bearing abstraction for everything that follows.
3. **Deputy threads, not distributed objects.** One kernel thread on home per away-process. Simple, debuggable, scales linearly. Distributed-object frameworks (CORBA-style) are tempting and wrong.
4. **Syscall classification at compile time.** Per-syscall flags in a static table. One branch in the dispatcher. Fast path stays fast.
5. **Every forwarded syscall is traceable.** The trace_id propagates across nodes. The Kernel Doctor can explain *exactly* why a process is slow. Without this, performance debugging in a distributed kernel is impossible.

---

## 9. What's Next

**Next document to write:** `dssi_06_live_migration.md` — Phases 20-22. Distributed scheduler (when to migrate, where, automatically), pre-copy migration (ship dirty pages iteratively while the process runs), post-copy migration (resume on target immediately, demand-fault pages), and hybrid pre+post-copy with compression. Pause time drops from seconds to tens of milliseconds. Migration becomes routine instead of a special operation.

After Doc 06, processes will move themselves without operator intervention, and you won't notice when they do.
