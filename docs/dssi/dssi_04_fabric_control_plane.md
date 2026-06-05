# DSSI 04 — Fabric Control Plane (Phases 16-17)

> **Scope:** Phases 16 and 17. The first phases where Kernel-V stops being a single-node OS and learns that *other nodes exist*. Nothing migrates yet, nothing executes remotely yet — but at the end of Phase 17 you can boot four nodes on a virtual network, watch them find each other, watch them gossip resource state, and watch one of them die and the others notice within ~2 seconds.
>
> **Position in the master plan:** Part 4 of 8. Foundation (Docs 00-01) gave us DSSI-shaped data structures. Observability (Doc 02) gave us eyes. Checkpoint/restore (Doc 03) gave us the ability to freeze and thaw a process. Now we build the *fabric*: the substrate on which everything distributed will run.
>
> **Mental model:** Think of Phases 16-17 as building the kernel's *nervous system between machines*. Not the spinal cord that carries motor commands (that's migration, Phases 18-22). Just the sensory network: each node continuously feels which other nodes are alive, how loaded they are, what they can offer. Without this, every later phase is impossible. With this, every later phase becomes a question of "what do we send over the wire we already have?"

---

## 0. Thinking Process — Why These Two Phases, In This Order, Now

Before diving into design, the reasoning for the phase boundary:

**Why a "control plane" before any "data plane"?**
Every distributed system fails at exactly two layers: membership (who is in the cluster *right now*) and failure detection (who *was* in the cluster but isn't anymore). Get these wrong and migrations corrupt processes, scheduling decisions target dead nodes, locks are held forever by ghosts. Get these right and everything else becomes *just* engineering.

The classic mistake — and one MOSIX partially made in its early versions — is to bolt membership onto the migration layer as an afterthought. Then every migration code path has to re-derive "is the target still alive?", and the answers diverge across code paths. The 2009 Cassandra paper (Lakshman & Malik) and the Akka cluster postmortems both call this out: **make membership a first-class subsystem with one authoritative view, then have everything consult it.**

**Why split membership (Phase 16) from gossip (Phase 17)?**
Two reasons:
1. Phase 16 is mostly *single-node and synchronous*: assigning identity, owning a network socket, accepting a single peer connection, exchanging "hello." It's small enough to write, test, and debug without distributed-system madness.
2. Phase 17 is *inherently asynchronous and partially-observable* (the hardest kind of code). Building it on top of a known-good identity + transport layer means when Phase 17 misbehaves, you know the bug is in your gossip logic, not in your socket handling.

This mirrors how Erlang/OTP layers its distribution: `:net_kernel` does node identity and pairwise links; `:global` does cluster-wide name registration on top. Two phases instead of one means each phase ships with smaller, more pointed tests.

**Why not skip ahead to actual migration?**
Tempting. The user's original vision is "transfer process execution to another node." Doc 03 just gave us checkpointing. Why not Phase 18 next?

Because *blind* migration is worse than no migration. If you don't know who's alive, what resources they have, or whether you can reach them, your migration will:
- Send a checkpoint to a node that crashed 200ms ago → process lost
- Migrate a CPU-heavy process to a node that's already at 100% → both nodes thrash
- Try to ship 4GB of pages to a peer with 100MB free RAM → OOM kill on receiver
- Pick a target by round-robin → ignore network topology, get 100x slower migrations

Phase 16-17 builds the *judgement* layer. Phase 18 builds the *action*. Confusing them is how distributed systems become unreliable.

**Why "fabric" and not "cluster"?**
A cluster implies central coordination — a master, a quorum, a "leader." A fabric is peer-to-peer, eventually-consistent, leaderless. Kernel-V's vision is a *global* OS that scales from 4 nodes in your closet to thousands of nodes across continents. The word matters because it drives every design decision: no master election, no quorum reads, no two-phase commits. Everything that can be eventually-consistent will be.

This is the same philosophy as Cassandra, Riak, and (in spirit) DynamoDB's storage layer. The cost is slightly stale information. The win is no single point of failure, no scalability ceiling, no split-brain disasters.

---

## 1. What These Phases Produce — The Concrete Deliverable

At the end of Phase 17, you can run this in QEMU:

```
$ make qemu-fabric-4node
[starts 4 QEMU VMs on a virtual bridge: node-a, node-b, node-c, node-d]

(on node-a console)
kv> fabric status
  Node ID:      a3f29c1e-... (node-a)
  Fabric state: JOINED
  Peers:        3 alive, 0 suspect, 0 dead
  Uptime:       00:02:14

kv> fabric peers
  ID          NAME    STATE   PHI    CPU%  MEMFREE   LAST_HEARD
  b9e2...     node-b  ALIVE   0.4    12%   240MB     180ms ago
  c4a1...     node-c  ALIVE   0.6    87%    12MB      90ms ago
  d8f7...     node-d  ALIVE   0.3    34%   180MB     220ms ago

kv> fabric ledger query node:c4a1 cpu_util_pct
  c4a1 cpu_util_pct: 87% (rolling 1s avg over last 30s)
  trend: ▁▂▄▆▇▇▇▆▇▇  (saturated, climbing)

(on node-c, simulate crash)
kv> panic

(back on node-a, ~2 seconds later)
[FABRIC] peer c4a1 (node-c) phi=8.5 -> SUSPECT
[FABRIC] peer c4a1 (node-c) phi=12.1 -> DEAD
[KV_TRACE] FABRIC_PEER_DOWN node=c4a1 reason=phi_threshold

kv> fabric peers
  ID          NAME    STATE   PHI    CPU%   MEMFREE   LAST_HEARD
  b9e2...     node-b  ALIVE   0.5    11%    240MB     150ms ago
  c4a1...     node-c  DEAD    --     --      --       3.2s ago
  d8f7...     node-d  ALIVE   0.4    35%    180MB     200ms ago
```

That's the entire visible behavior. Internally we have:
- A `kv_node_id` UUID assigned at first boot, persisted across reboots
- A fabric link socket per peer (one TCP connection, message-framed)
- A gossip subsystem rotating peer state every ~200ms
- A Phi Accrual failure detector continuously scoring each peer
- A `fabric_ledger` integrated with the Resource Ledger from Doc 02
- Shell tooling to inspect everything

No process has been migrated. No syscall has been forwarded. But the foundation that all of that will sit on top of is now in place and observable.

---

## 2. Phase 16 — Fabric Identity & Node Bootstrap

### 2.1 Goals

1. Every node has a stable, globally-unique `kv_node_id` (128-bit UUID).
2. The ID persists across reboots, but is auto-regenerated on first boot.
3. A configurable list of seed peers can be loaded at boot.
4. A node can open a TCP connection to a seed peer and exchange a HELLO message.
5. After HELLO exchange, both nodes have a `kv_fabric_peer_t` record for each other.
6. A node can ungracefully crash, and its peers will (eventually) close their stale link sockets — though *detection* and *liveness scoring* is Phase 17.

### 2.2 Why Identity Comes Before Anything Else

A node's identity is the foundation of the `kv_gid_t` we baked into every kernel object back in Doc 01:

```c
typedef struct kv_gid {
    uint64_t node_id;     // 0 = local, nonzero = remote
    uint64_t local_id;
} kv_gid_t;
```

Until Phase 16, every `kv_gid_t` we created had `node_id = 0`. Phase 16 makes `node_id` real. From this point on, every object created inside the kernel will be stamped with `node_id = my_kv_node_id_64bit_form`, and remote objects we learn about will carry the remote node's ID.

The 128-bit UUID is the *human-and-fabric-readable* identity. The 64-bit `node_id` inside `kv_gid_t` is a *derived hash* (specifically a SipHash-2-4 of the UUID with a process-wide key) chosen so that:
- Collisions are vanishingly unlikely (birthday paradox kicks in around 2^32 ≈ 4 billion nodes; we'll never run that many)
- Lookup tables can index by the 64-bit form
- The full UUID is preserved for human display and cryptographic operations (later, Phase 27)

### 2.3 Identity Generation & Persistence

**First boot:**
```c
kv_node_uuid_t kv_node_id_generate(void) {
    kv_node_uuid_t id;
    // 16 random bytes from the entropy pool
    // (Phase 12 trace ring buffer + TSC jitter give us ~30 bits per second
    //  of low-quality entropy; that's plenty for a UUID on first boot)
    kv_random_bytes(&id, sizeof(id));
    // Set version (4) and variant bits per RFC 4122
    id.bytes[6] = (id.bytes[6] & 0x0F) | 0x40;
    id.bytes[8] = (id.bytes[8] & 0x3F) | 0x80;
    return id;
}
```

**Persistence:** This is where Phase 16 makes its first scary commitment: we need to write something to disk that survives reboots. Until now, every boot of Kernel-V has been ephemeral.

We'll use the block I/O layer from Phase 7 to write a tiny "node configuration block" at LBA 1 of the boot disk (LBA 0 is the MBR; LBA 1 is conventionally free in our setup). The block is:

```c
typedef struct kv_node_config_block {
    uint32_t magic;           // 'KVNC' = 0x4B564E43
    uint32_t version;         // 1
    kv_node_uuid_t node_id;
    char     node_name[64];   // human-readable, e.g. "node-a"
    uint64_t first_boot_ts;   // when this ID was generated
    uint32_t reserved[48];
    uint32_t crc32;           // over everything above
} __attribute__((packed)) kv_node_config_block_t;
// Exactly 256 bytes, fits in a single 512-byte sector with room to spare
```

**Boot sequence change:**
1. Read LBA 1 from boot disk.
2. If magic + CRC valid → load `kv_node_id` from it.
3. If magic invalid → generate new ID, prompt user for name (or use `node-<hex prefix>`), write block.
4. Either way, derive the 64-bit `node_id` for `kv_gid_t` via SipHash-2-4 of the UUID.

**`KV_TRACE` events emitted:**
- `FABRIC_IDENTITY_LOADED` (with UUID, name, first-boot timestamp)
- `FABRIC_IDENTITY_GENERATED` (first-boot case only)

### 2.4 Bootstrap Configuration — Seed Peers

Like Cassandra, Consul, and Akka, we use a "seed peers" approach. A new node needs to know about *at least one* existing fabric member; once connected, gossip (Phase 17) will tell it about all the others.

Seed list is read from the node config block (extended for v2), or from a `fabric.cfg` file on the boot disk (a deliberately simple text format):

```
# /etc/fabric.cfg — one peer per line
# format: <ip_or_hostname>:<port>
10.0.0.2:7373
10.0.0.3:7373
```

For now (Phase 16), we hard-code the seed list at compile time for the test setup. The file-based version is a Phase 17 polish item.

### 2.5 Network Transport — The First Real Network Code

Phase 11 (in `kernel_plan.md`) introduced the kernel network stack with two paths: user TCP/IP for normal sockets, and a *fabric link* path with a custom protocol. Phase 16 is where the fabric link path actually does something.

**Transport choice for now: TCP.**
Yes, eventually we want a custom QUIC-inspired reliable datagram protocol (Phase 25), but for the first working version, TCP is correct because:
- We already have it from Phase 11
- Head-of-line blocking doesn't matter at gossip volumes (~few KB/s per peer)
- Loss recovery, ordering, congestion control are all already solved
- We can framing-prefix every message with a `uint32_t length` and never think about it again

**The framing:**
```c
typedef struct kv_fabric_msg_hdr {
    uint32_t magic;        // 'KVFM' = 0x4B56464D
    uint32_t length;       // total bytes including this header
    uint32_t msg_type;     // see kv_fabric_msg_type_t
    uint32_t flags;
    uint64_t sender_node_id;   // 64-bit form
    uint64_t sequence;         // per-link sequence, for diagnostics
    uint64_t timestamp_tsc;    // sender's TSC at send time
} __attribute__((packed)) kv_fabric_msg_hdr_t;
```

**Message types for Phase 16:**
- `KV_FMSG_HELLO` (0x01) — first message after TCP connect, carries full UUID, name, version
- `KV_FMSG_HELLO_ACK` (0x02) — response with peer's identity
- `KV_FMSG_GOODBYE` (0x03) — graceful disconnect signal (rare in practice)
- `KV_FMSG_PING` (0x04) — minimal heartbeat (used as scaffold for Phase 17 gossip)

### 2.6 The HELLO Handshake

```
Node A (initiator)                         Node B (acceptor)
    |                                            |
    |---- TCP SYN -------------------------->    |
    |<--- TCP SYN-ACK -----------------------    |
    |---- TCP ACK -------------------------->    |
    |                                            |
    |---- KV_FMSG_HELLO -------------------->    |
    |     {uuid_A, name_A, version_A,            |
    |      fabric_features_A}                    |
    |                                            |
    |                                            | [validate, dedupe by uuid]
    |                                            | [allocate kv_fabric_peer_t]
    |                                            |
    |<--- KV_FMSG_HELLO_ACK -----------------    |
    |     {uuid_B, name_B, version_B,            |
    |      fabric_features_B}                    |
    |                                            |
    | [validate, allocate kv_fabric_peer_t]      |
    | [link state: ESTABLISHED]                  |
    |                                            |
    | ... ready for PINGs / gossip ...           |
```

Failure paths to handle now (the boring but critical ones):
- TCP connect refused → retry with exponential backoff (1s, 2s, 4s, 8s, max 60s)
- HELLO version mismatch → log, close, don't retry until config change
- Same UUID as ourselves → log loudly, close (probably a misconfigured clone)
- Same UUID as already-connected peer → close newer connection, keep older (prevents flapping)
- HELLO timeout (no response in 5s) → close, retry as above

### 2.7 The `kv_fabric_peer_t` Structure

This is the core in-memory record of "someone we know about":

```c
typedef enum kv_fabric_peer_state {
    KV_FPS_DISCOVERED = 0,    // we know they exist but haven't connected
    KV_FPS_CONNECTING,        // TCP connect in progress
    KV_FPS_HANDSHAKING,       // TCP up, HELLO not yet complete
    KV_FPS_ALIVE,             // fully connected and recently heard from
    KV_FPS_SUSPECT,           // phi rising — see Phase 17
    KV_FPS_DEAD,              // phi past threshold — see Phase 17
    KV_FPS_GONE,              // graceful goodbye, do not auto-reconnect
} kv_fabric_peer_state_t;

typedef struct kv_fabric_peer {
    kv_node_uuid_t          uuid;
    uint64_t                node_id_64;       // SipHash of uuid
    char                    name[64];
    uint32_t                version;
    uint32_t                fabric_features;  // bitmask, future use

    // Network endpoint
    struct sockaddr_storage addr;
    struct kv_socket       *link_sock;        // NULL if not connected
    kv_fabric_peer_state_t  state;

    // Liveness — populated in Phase 17
    struct kv_phi_accrual   phi;              // see §3.6
    uint64_t                last_heard_tsc;
    uint64_t                heard_count;

    // Resource snapshot — populated in Phase 17
    struct kv_resource_snapshot resources;

    // Send/receive queues (lock-protected)
    struct kv_msg_queue     send_q;
    struct kv_msg_queue     recv_q;
    spinlock_t              lock;

    // List linkage
    struct list_head        all_peers_link;
    uint64_t                discovered_ts;
} kv_fabric_peer_t;
```

The global peer table is a hash table keyed on `node_id_64` plus a list head for iteration. Lookups are O(1); enumeration for shell commands is O(n).

### 2.8 Threading Model — Critical Design Decision

Each fabric link runs two kernel threads:
- `fabric_link_rx_<peer>` — blocks on `recv()`, parses frames, dispatches to handlers
- `fabric_link_tx_<peer>` — blocks on send queue condvar, drains messages to socket

Plus one global thread:
- `fabric_supervisor` — periodically rescans seed list, attempts reconnects to `GONE`/closed peers, garbage-collects `DEAD` peers after a long timeout (e.g., 1 hour)

This is the same threading model Akka uses, and it's chosen because:
- TX and RX can block independently without deadlocking
- Per-peer threads mean one slow peer can't stall others
- Supervisor isolation means link failures can't crash the supervisor itself

Cost: ~16KB of kernel stack per peer (8KB × 2 threads). At 1000 peers that's 16MB. Acceptable.

### 2.9 Shell Commands (Phase 16 set)

```
fabric status                  # local node id, name, fabric state summary
fabric peers                   # list with state, name, address
fabric connect <ip>:<port>     # manual peer add (debugging)
fabric disconnect <name>       # graceful link teardown
fabric send-ping <name>        # one-shot diagnostic ping
```

All output flows through the same trace + ledger infrastructure as Doc 02. Every command emits trace events; every result can be replayed from the trace ring.

### 2.10 Test Strategy for Phase 16

Three test tiers, in order:

1. **Single-node identity test:** boot a node twice, confirm same UUID both times. Wipe LBA 1, confirm new UUID generated.
2. **Two-node HELLO test:** boot two nodes, confirm they connect and both report each other in `fabric peers`.
3. **Resilience tests:**
   - Kill node B's process; confirm node A's link transitions to error state within 30s (no Phi yet, just TCP error)
   - Restart node B; confirm node A reconnects automatically
   - Network partition (drop iptables rule); confirm graceful recovery when partition heals

### 2.11 Phase 16 Implementation Phasing (4 weeks)

| Week | Deliverable | Verifiable result |
|------|-------------|-------------------|
| 1 | Node identity: UUID generation, config block read/write, SipHash | Boot prints `node_id`, persists across reboot |
| 2 | Fabric link transport: socket setup, message framing, HELLO handshake | Two nodes can `fabric connect` and exchange HELLO |
| 3 | Peer state machine, supervisor thread, retry logic | Killed peer disconnects cleanly, auto-reconnect works |
| 4 | Shell commands, trace integration, test suite, polish | `make qemu-fabric-2node` demo works end-to-end |

---

## 3. Phase 17 — Gossip Protocol & Resource Advertisement

### 3.1 Goals

1. Every node periodically broadcasts (via gossip) a snapshot of its current resource state.
2. Every node maintains a *probabilistic eventually-consistent* view of every other node's state.
3. New peers are discovered transitively: A is connected to B, B is connected to C — A learns of C through gossip and connects automatically.
4. Failure is detected within ~2 seconds of a node going silent, via a **Phi Accrual** scoring function (not a hard timeout).
5. The Resource Ledger from Doc 02 is extended to include per-peer time series.

### 3.2 Why Gossip, Not Broadcast or Centralized Heartbeats

Three options were on the table:

| Option | How it works | Why we rejected it |
|--------|--------------|---------------------|
| **Central coordinator** | One "leader" node collects state and broadcasts | Single point of failure. Election overhead. Doesn't scale. |
| **All-to-all heartbeats** | Every node pings every other node directly | O(n²) connections. 100 nodes = 10,000 sockets. Falls apart. |
| **Gossip (epidemic protocol)** | Each node periodically tells a random subset of peers what it knows | O(n log n) convergence, O(n) state, no SPOF. **Pick this.** |

The seminal paper here is Demers et al., "Epidemic Algorithms for Replicated Database Maintenance" (PODC 1987). The modern systems we're learning from are Cassandra (early generations), Consul, Serf, Akka cluster, and the SWIM paper (Das/Gupta/Motivala 2002) which Consul/Serf are based on.

**We will use SWIM-style gossip with one significant modification:** instead of SWIM's binary alive/dead model, we use Phi Accrual (§3.6) for continuous suspicion scoring. This gives us much smoother degradation and far fewer false positives than SWIM's three-second timeout heuristic.

### 3.3 The Gossip Round

Every `KV_GOSSIP_INTERVAL_MS` (default: 200ms — chosen to balance freshness vs. bandwidth, same as Consul's default):

```c
void fabric_gossip_round(void) {
    // 1. Update our own resource snapshot
    kv_resource_snapshot_t my_snap = kv_collect_my_resources();

    // 2. Choose K random alive peers (default K=3 — SWIM's "fanout")
    kv_fabric_peer_t *targets[KV_GOSSIP_FANOUT];
    int n = kv_random_alive_peers(targets, KV_GOSSIP_FANOUT);

    // 3. Build a gossip payload
    kv_gossip_payload_t payload;
    payload.sender_snapshot = my_snap;
    payload.known_peers = kv_select_peer_subset_for_gossip();

    // 4. Send to each target
    for (int i = 0; i < n; i++) {
        fabric_send(targets[i], KV_FMSG_GOSSIP, &payload, sizeof(payload));
    }

    // 5. Update Phi scores for ALL peers based on time-since-last-heard
    fabric_update_all_phi();

    // 6. Transition states if Phi crossed thresholds
    fabric_check_phi_transitions();
}
```

K=3 is the magic number from SWIM analysis: with K=3 and a population of N, the expected time for a piece of news to reach all nodes is O(log N) rounds. For N=100, that's ~5 rounds = 1 second at our 200ms interval. For N=10,000, that's ~10 rounds = 2 seconds. Logarithmic scaling is what makes gossip beautiful.

### 3.4 The Gossip Payload

```c
typedef struct kv_resource_snapshot {
    uint64_t timestamp_tsc;       // when this snapshot was taken (sender's clock)
    uint64_t wall_time_ms;        // best-effort wall-clock, for human display

    // CPU
    uint32_t cpu_util_pct;        // 0-100, rolling 1s average
    uint32_t cpu_count;           // number of logical CPUs
    uint32_t loadavg_x100;        // 1-minute loadavg × 100 for fixed-point

    // Memory
    uint64_t mem_total_kb;
    uint64_t mem_free_kb;
    uint64_t mem_dirty_kb;        // currently-dirty pages
    uint64_t mem_cached_kb;

    // Processes
    uint32_t proc_count;
    uint32_t proc_running;
    uint32_t proc_blocked;

    // Fabric-specific
    uint32_t migrations_in_progress;   // (will be used Phase 18+)
    uint32_t reserved_for_migrations_mb;

    // Health
    uint32_t self_phi_x100;       // (not used; placeholder for self-assessment)
    uint32_t hot_event_count;     // KV_TRACE events in last second
} kv_resource_snapshot_t;  // ~96 bytes
```

The "known peers" portion of the gossip payload carries a *digest* of each peer the sender knows, not the full state:

```c
typedef struct kv_peer_digest {
    uint64_t       node_id_64;
    uint64_t       last_seen_ts;  // when the sender last heard from them
    uint8_t        state;         // sender's view: ALIVE/SUSPECT/DEAD
    uint8_t        version;       // monotonic per-peer-snapshot counter
    uint16_t       _pad;
    // (No resource snapshot here; receivers ask separately if interested)
} kv_peer_digest_t;  // 16 bytes
```

Digest-then-request is the **anti-entropy** pattern from Cassandra: cheap digests over the wire, full state only when versions differ. For a fabric of 1000 nodes, each gossip message is `96 + 1000×16 = ~16KB`. At 200ms intervals × 3 fanout = `~240 KB/s` of gossip traffic per node — negligible on any modern network.

### 3.5 Discovery via Transitive Gossip

When node A receives a gossip payload from node B containing a digest of node C that A doesn't know about:

1. A allocates a `kv_fabric_peer_t` for C in state `DISCOVERED`.
2. A asks B for C's contact information (`KV_FMSG_PEER_INFO_REQUEST`).
3. B responds with C's address (`KV_FMSG_PEER_INFO_RESPONSE`).
4. A initiates an outbound connection to C (same flow as §2.6).

This is how a fabric grows: you only need to seed one connection, and the rest of the membership graph fills in automatically. It's also how the fabric heals after a network partition — once a single cross-partition link reopens, gossip propagates the merged membership.

### 3.6 Phi Accrual Failure Detection

This is the most subtle piece of Phase 17, and the one most likely to be wrong if implemented naively. So let's dig in.

**The problem with hard timeouts:** if you say "a peer is dead if I haven't heard from them in 1500ms," you've made a binary decision based on a single threshold. In a real network:
- Brief GC pause on the peer (300ms)? Looks alive.
- One dropped packet during heavy load (500ms recovery)? Looks alive.
- Sustained 1400ms latency due to network congestion? Looks alive (barely).
- A 1501ms blip? **DEAD.** Application gets a callback, fails over, migrates state — and 50ms later the peer responds normally. Classic false positive disaster.

**The Phi Accrual approach** (Hayashibara et al., "The φ Accrual Failure Detector," 2004): instead of a binary alive/dead decision, compute a continuously-valued *suspicion level* φ, where:

```
φ(t) = -log10( P(message arrival time > t | history) )
```

In English: φ is the negative log probability that the gap since the last heartbeat is "normal" given the historical distribution of gaps. If gaps are usually 200ms and we've been silent for 250ms, that's not surprising — φ ~ 0.5. If gaps are usually 200ms and we've been silent for 5000ms, that's *extremely* surprising — φ ~ 10.

The application chooses a threshold (Cassandra's default is φ=8, our default is φ=8 too). At φ < 8, peer is `ALIVE`. At 8 ≤ φ < 12, peer is `SUSPECT` (don't migrate to it, but don't tear down state either). At φ ≥ 12, peer is `DEAD`.

**Implementation:**

```c
#define PHI_WINDOW_SIZE 100   // last 100 inter-arrival samples

typedef struct kv_phi_accrual {
    uint64_t  samples[PHI_WINDOW_SIZE];   // inter-arrival times in ms
    int       sample_count;               // <= PHI_WINDOW_SIZE
    int       sample_head;                // circular buffer head
    uint64_t  sum;                        // sum of samples
    uint64_t  sum_squares;                // for stddev
    uint64_t  last_arrival_tsc;
} kv_phi_accrual_t;

void phi_arrival(kv_phi_accrual_t *p, uint64_t now_tsc) {
    if (p->last_arrival_tsc != 0) {
        uint64_t gap_ms = tsc_to_ms(now_tsc - p->last_arrival_tsc);

        // Add new sample, evict oldest if window full
        if (p->sample_count == PHI_WINDOW_SIZE) {
            uint64_t old = p->samples[p->sample_head];
            p->sum -= old;
            p->sum_squares -= old * old;
        } else {
            p->sample_count++;
        }
        p->samples[p->sample_head] = gap_ms;
        p->sum += gap_ms;
        p->sum_squares += gap_ms * gap_ms;
        p->sample_head = (p->sample_head + 1) % PHI_WINDOW_SIZE;
    }
    p->last_arrival_tsc = now_tsc;
}

double phi_score(kv_phi_accrual_t *p, uint64_t now_tsc) {
    if (p->sample_count < 10) return 0.0;  // not enough data yet

    uint64_t t_ms = tsc_to_ms(now_tsc - p->last_arrival_tsc);
    double mean = (double)p->sum / p->sample_count;
    double var = ((double)p->sum_squares / p->sample_count) - (mean * mean);
    double stddev = sqrt(var);
    if (stddev < 1.0) stddev = 1.0;  // floor to avoid div-by-zero spikes

    // Normal CDF complement, approximated
    double z = (t_ms - mean) / stddev;
    double p_later = 0.5 * erfc(z / sqrt(2.0));
    if (p_later < 1e-15) p_later = 1e-15;

    return -log10(p_later);
}
```

**Floating-point math in kernel space?** Yes, but only in this one subsystem and only at gossip rate (200ms). We isolate it with `kernel_fpu_begin()` / `kernel_fpu_end()` (Phase 11 introduced this for the network stack's checksum offload). The cost is negligible.

**Why this is *much* better than heartbeats:**
- On a quiet, low-jitter network, it converges fast: φ ramps from 0 to 8 in ~1s
- On a congested, high-jitter network, it tolerates: a peer can be 3× normal latency without tripping
- It's *adaptive*: a peer that's always slow doesn't get flagged; a normally-fast peer that suddenly goes silent gets flagged immediately

This is the same algorithm Cassandra uses, with the same window size, with the same thresholds. It's well-trodden ground.

### 3.7 Resource Ledger Integration

The Resource Ledger from Doc 02 was designed (deliberately) to be per-node-aware. Phase 17 wires up the cross-node piece:

```c
// Every time we receive a gossip payload from peer P:
kv_ledger_record_remote(
    p->node_id_64,
    "cpu_util_pct",
    snapshot->cpu_util_pct,
    snapshot->timestamp_tsc
);
// ... similar for mem_free_kb, loadavg, etc.
```

Now `ledger query node:c4a1 cpu_util_pct` works for any peer, not just localhost. The ledger handles staleness markers automatically: if we haven't heard from a peer in N×gossip_interval, queries return `STALE(<age>)` rather than the last value.

### 3.8 Anti-Entropy Reconciliation

Gossip-on-its-own can be slow to converge under heavy churn. Every 10 seconds, run a full *anti-entropy* round:

1. Pick one random peer.
2. Send our full peer table as a digest list.
3. Receive their full digest list.
4. For each peer where versions differ, exchange full state.

This guarantees convergence even if regular gossip drops messages. It's the same mechanism Cassandra uses for SSTable repair, applied to membership instead of data.

### 3.9 Membership View Convergence — A Worked Example

Five nodes: A, B, C, D, E. A and B are connected initially. C, D, E come online in quick succession, each seeded with B's address.

```
t=0.0s   A↔B established.
t=0.5s   C connects to B. B knows {A, C}. A still knows {B}.
t=0.7s   Gossip round on B: tells A about C.
         A creates DISCOVERED entry for C, requests address.
t=0.9s   A connects to C.
         Now A, B, C all know each other.
t=1.0s   D connects to B. Same process.
t=1.5s   E connects to B. Same process.
t=2.0s   All five nodes have all five peers in ALIVE state.
         Total convergence time: ~2 seconds.
```

This is what "eventual consistency" looks like in practice: not instant, but bounded and predictable. The bound is `(diameter of gossip graph) × (gossip interval)`, which for a fanout-3 gossip is `log_3(N)` rounds = `log_3(N) × 200ms`. For N=10,000, that's ~1.8s of theoretical convergence time. In practice, with some redundancy, ~3-5s.

### 3.10 New Message Types for Phase 17

| Type | Purpose | Frequency |
|------|---------|-----------|
| `KV_FMSG_GOSSIP` | Periodic state share | every 200ms |
| `KV_FMSG_PEER_INFO_REQUEST` | "Tell me how to reach peer X" | rare, on discovery |
| `KV_FMSG_PEER_INFO_RESPONSE` | Address of peer X | response to above |
| `KV_FMSG_ANTI_ENTROPY` | Full digest exchange | every 10s |
| `KV_FMSG_RESOURCE_QUERY` | "Send full snapshot now" | rare, on demand |
| `KV_FMSG_RESOURCE_RESPONSE` | Full snapshot | response to above |

### 3.11 Shell Commands (Phase 17 additions)

```
fabric gossip-stats               # messages sent/received, bytes, dropped
fabric ledger query <node> <key>  # cross-node ledger queries
fabric phi <node>                 # current phi score + sample history
fabric topology                   # who is connected to whom (best effort)
fabric simulate-partition <list>  # debugging: drop messages to specified peers
```

### 3.12 What Phase 17 Deliberately Does NOT Do

- **No leader election.** We don't have one because we don't need one yet.
- **No quorum membership.** Gossip is eventually consistent; that's the design.
- **No security.** Anyone on the network can join and lie about resources. Phase 27 fixes this.
- **No NAT traversal.** All nodes are assumed to be on routable IPs. Real internet-scale deployment is Phase 28.
- **No migration.** That's literally what Phase 18 is for.
- **No process awareness.** Gossip carries node-level resources, not per-process visibility. Per-process distributed state is Phase 20+.

### 3.13 Test Strategy for Phase 17

1. **Convergence test:** boot 4 nodes, time how long until all four agree on membership. Should be <5s.
2. **Phi calibration test:** introduce artificial 500ms one-way latency on one peer link; confirm phi rises but doesn't trigger SUSPECT. Then introduce 5000ms; confirm SUSPECT within 1-2s, DEAD within 3-4s.
3. **Partition test:** split the 4-node cluster into {A,B} and {C,D}. Confirm:
   - Within ~3s, A and B mark C and D as DEAD.
   - C and D mark A and B as DEAD.
   - Heal the partition. Within ~5s, all four are ALIVE again.
4. **Churn test:** in a 4-node cluster, randomly kill and restart one node every 30s for 10 minutes. Confirm membership view eventually correct in all surviving nodes after each event.
5. **Bandwidth test:** measure gossip traffic at 4, 16, 64 simulated nodes (use one box, many QEMU instances). Confirm sub-linear growth in per-node bandwidth.

### 3.14 Phase 17 Implementation Phasing (5 weeks)

| Week | Deliverable | Verifiable result |
|------|-------------|-------------------|
| 1 | Gossip round loop, payload format, send/receive plumbing | Two nodes exchange gossip every 200ms, ledger shows peer's CPU% |
| 2 | Phi accrual: sample collection, score calculation, state transitions | Kill a peer, watch phi rise, state goes ALIVE→SUSPECT→DEAD |
| 3 | Transitive discovery, peer info request/response | Boot 3 nodes with linear seeding; all three converge to full mesh |
| 4 | Anti-entropy, partition healing, churn handling | Partition test passes; churn test passes |
| 5 | Resource ledger cross-node integration, shell commands, polish | Full 4-node demo from §1 works |

---

## 4. The `KV_TRACE` Events Added in These Phases

Adding to the event taxonomy from Doc 02:

```c
// Category: FABRIC (0x40)
#define KV_TRACE_FABRIC_IDENTITY_LOADED      0x4001
#define KV_TRACE_FABRIC_IDENTITY_GENERATED   0x4002
#define KV_TRACE_FABRIC_LINK_CONNECTING      0x4010
#define KV_TRACE_FABRIC_LINK_HANDSHAKING     0x4011
#define KV_TRACE_FABRIC_LINK_ESTABLISHED     0x4012
#define KV_TRACE_FABRIC_LINK_CLOSED          0x4013
#define KV_TRACE_FABRIC_LINK_ERROR           0x4014
#define KV_TRACE_FABRIC_PEER_DISCOVERED      0x4020
#define KV_TRACE_FABRIC_PEER_ALIVE           0x4021
#define KV_TRACE_FABRIC_PEER_SUSPECT         0x4022
#define KV_TRACE_FABRIC_PEER_DEAD            0x4023
#define KV_TRACE_FABRIC_PEER_RECOVERED       0x4024
#define KV_TRACE_FABRIC_GOSSIP_SENT          0x4030
#define KV_TRACE_FABRIC_GOSSIP_RECEIVED      0x4031
#define KV_TRACE_FABRIC_ANTI_ENTROPY_RUN     0x4032
#define KV_TRACE_FABRIC_PHI_THRESHOLD        0x4040
```

These integrate with the Kernel Doctor from Doc 02 so you can ask:

```
kv> doctor explain "why was node-c marked dead?"
[FABRIC_PEER_DEAD c4a1] at 10:23:45.231
  caused by: KV_TRACE_FABRIC_PHI_THRESHOLD (phi=12.8, threshold=12.0)
  caused by: KV_TRACE_FABRIC_LINK_ERROR (errno=ETIMEDOUT) at 10:23:42.115
  caused by: 3 missed gossip rounds starting at 10:23:41.300
  last gossip received: 10:23:41.105 (3.4s before death declaration)
  peer's last reported state: CPU 87%, mem_free 12MB
```

This is why we built TraceOS *before* we built the fabric.

---

## 5. How These Phases Set Up Phase 18 (Stop-and-Copy Migration)

When we begin Phase 18 in Doc 05, here's what we'll already have:

1. **A target selection oracle:** "Pick a peer with state=ALIVE, phi<2.0, cpu_util_pct<40, mem_free_kb>(my_process_size*1.5)". The ledger lets us answer this in O(n) over the peer set.

2. **A reliable byte pipe:** We can call `fabric_send(peer, KV_FMSG_MIGRATION_CHECKPOINT, data, len)` to ship a checkpoint image (from Doc 03) to any alive peer.

3. **A pre-flight safety check:** Before sending the checkpoint, we can verify the peer is still alive by checking its phi score (must be <2.0 for migration eligibility).

4. **An in-flight liveness monitor:** During the migration, we can monitor the receiver's phi; if it spikes during the transfer, we abort and try elsewhere.

5. **A post-migration confirmation channel:** The receiver acknowledges checkpoint completion via a fabric message; we can verify before tearing down the source-side process.

Without Phases 16-17, every one of these would be ad-hoc code in the migration path. With them, migration becomes a thin protocol on top of a solid foundation.

---

## 6. Risks & Mitigations

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Floating-point in kernel space causes weird bugs | Medium | Strictly bracketed with `kernel_fpu_begin/end`; tested in isolation |
| Gossip storms during partition healing | Medium | Cap gossip-target selection randomness; throttle peer_info requests |
| TCP head-of-line blocking causes phi false positives | Low | Phi window is large enough (100 samples) to smooth over brief stalls |
| Clock skew between nodes confuses timestamps | High | Use only TSC for local timing; wall-clock is best-effort display only |
| Persistent node config block corruption | Low | CRC32 + magic + version triple-check; on failure, prompt human for ID |
| 1000-node fabric overwhelms gossip bandwidth | Low | Anti-entropy uses digest-then-request; bandwidth is `O(N)` not `O(N²)` |
| Misconfigured duplicate UUIDs | Medium | HELLO rejects same-UUID-as-self; logs loudly so operator notices |

---

## 7. Aggregate Timeline & Position in the Roadmap

| Phase | Weeks | Cumulative |
|-------|-------|------------|
| Phase 16: Fabric Identity & Bootstrap | 4 | 4 |
| Phase 17: Gossip & Resource Advertisement | 5 | 9 |

Combined with the prior docs:
- Doc 01 (Phases 6-11): UNIX foundation with DSSI seeds — happens in parallel/before
- Doc 02 (Phases 12-13): TraceOS + Resource Ledger — 9 weeks
- Doc 03 (Phases 14-15): Slab allocator + Checkpoint/Restore — 13 weeks
- **Doc 04 (Phases 16-17): Fabric Control Plane — 9 weeks** ← we are here
- Doc 05 (Phases 18-19): Stop-and-copy + Syscall proxy — first DSSI demo

At the end of Phase 17 you have **the foundation for everything that follows**. The next document is where the vision becomes visible.

---

## 8. The Five Most Important Decisions in This Document

If you only remember five things from Doc 04:

1. **Identity is persistent, derived twice.** UUID on disk, 64-bit SipHash in `kv_gid_t`. Both are needed.
2. **Membership is gossip, not consensus.** Eventual consistency with bounded convergence time. No leader, no quorum.
3. **Failure detection is Phi Accrual, not timeouts.** Continuous suspicion score with adaptive thresholds. Cassandra-tested.
4. **Resource state piggybacks on gossip.** No separate "metrics push" channel. One pipe, one cadence.
5. **TraceOS observes everything fabric does.** Every state transition is a trace event. The Kernel Doctor can explain any membership decision.

---

## 9. What's Next

**Next document to write:** `dssi_05_first_remote_exec.md` — Phases 18-19. Stop-and-copy process migration on top of the fabric, plus syscall proxying (the home-node "deputy" mechanism from MOSIX). This is **the first phase where the user's original vision actually works**: a process running on node A pauses, copies to node B, resumes on node B, and continues making syscalls that the user wouldn't be able to distinguish from local execution.

Phase 18 is where Kernel-V becomes a distributed operating system.
