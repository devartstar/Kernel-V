# Phase 2 — The Fabric: from network wire to kernel RPC

**One correction to the roadmap:** there is no virtio code in your tree. You do have the PCI bus and driver binding, BAR and capability parsing, `dma_alloc_pages`, MMIO helpers, IRQ registration, the workqueue and ktimers. So we build the virtio layer ourselves before the network card driver.

**Goal:** two (later N) nodes exchange authenticated, reliable, prioritized messages and call each other's kernel functions with deadlines.
- **M2** (a reliable message gets through under packet loss) is reached at the end of N3.
- **M3** (one node calls a kernel function on another) is reached at the end of N4.

## New ideas this design brings
1. **Header that doesn't care about the carrier:** the same header works over raw Ethernet today and over UDP or RDMA later.
2. **simwire:** a simulated wire that drops, duplicates, reorders, delays and corrupts packets based on a seed. Any transport bug can be replayed exactly from its seed.
3. **Boot incarnations:** a number that changes each boot. If a peer reboots, we notice within one round trip, and calls in progress fail fast instead of hanging.
4. **Lanes:** separate priority queues, so typing in a terminal never waits behind a bulk process-migration transfer.
5. **Deadline budgets:** a call carries "time remaining", not a clock time, so nodes don't need synchronized clocks.
6. **At-most-once calls:** each node keeps a cache of recent replies, so a retried call is never executed twice.
7. **Cross-node tracing:** a trace ID travels with each call, so logs from node A and node B can be joined.
8. **Authenticated frames from the first byte**, as the security rule in your roadmap requires.
9. **Load in discovery beacons:** the "I'm here" messages also report each node's load, giving the future scheduler its first cluster view at no extra cost.
10. **`/net/peers` as a file:** the first appearance of the `/net` namespace.

## Phases
| Phase | Subphases | Exit test |
|---|---|---|
| **N0 Setup and building blocks** | 0.1 Update the roadmap · 0.2 `kthread_create` · 0.3 Two-node QEMU setup (`-netdev dgram`, packet captures you can open in Wireshark) · 0.4 `netbuf` packet buffer (space reserved for headers, reference counted, preallocated pool) · 0.5 Byte-order helpers and bounds-checked read/write cursor | Two QEMU nodes boot, network cards appear on PCI, unit tests pass |
| **N1 Virtio + virtio-net driver** | 1.1 Modern virtio-PCI setup and handshake · 1.2 Virtqueue library, tested against a fake device · 1.3 virtio-net driver detection · 1.4 `netdev` interface plus a loopback device · 1.5 Receive path (interrupt hands work to a worker thread; interrupts paused while it drains) · 1.6 Send path with backpressure when the ring is full · 1.7 Tests | Nodes exchange raw Ethernet frames |
| **N2 Fabric link layer (KVF)** | 2.1 Ethernet parsing and dispatch by EtherType (0x88B5) · 2.2 KVF header design · 2.3 Node ID and boot incarnation · 2.4 Discovery beacons and neighbor table · 2.5 Frame authentication with SipHash · 2.6 Wireshark plugin | Send to "node B" by ID; forged or corrupted frames are dropped and counted |
| **N3 Reliable transport (KVT)** | 3.0 simwire first · 3.1 Peer connection states and handshake · 3.2 Sequence numbers and acknowledgements, including which out-of-order packets arrived · 3.3 Retransmission with a timeout learned from measured round-trip times · 3.4 Splitting and reassembling large messages · 3.5 Lanes · 3.6 Flow and congestion control · 3.7 Peer UP/SUSPECT/DOWN tracking · 3.8 Tests across many seeds | **M2:** a 256 KB message arrives intact under injected loss |
| **N4 Kernel RPC (KRPC)** | 4.1 Message encoding · 4.2 Service registry and worker pool · 4.3 Blocking and async calls · 4.4 Reply cache for at-most-once · 4.5 Deadline budgets and cancellation · 4.6 Trace propagation · 4.7 Built-in `sys.ping`/`echo`/`info` · 4.8 Tests | **M3:** remote echo works; a non-idempotent call runs exactly once under loss |
| **N5 Visibility and demo** | 5.1 `/net/peers` and `/net/stats` files · 5.2 Counters and latency histograms · 5.3 Benchmarks · 5.4 Demo and roadmap update | Recorded M2/M3 demos |

## Decisions for you to confirm
| # | Choice | My recommendation |
|---|---|---|
| D1 | Modern virtio (1.x) or legacy | **Modern** |
| D2 | Raw Ethernet or UDP/IP | **Raw Ethernet now**, with the carrier-independent header |
| D3 | QEMU network backend | **`dgram`** (no root needed) |
| D4 | Byte order of our message contents | **Little-endian** (Ethernet's own fields stay big-endian) |
| D5 | Frame authentication | **SipHash now**, full encryption in S1 |
| D6 | Message-based or byte-stream transport | **Messages plus lanes** |

The plan is saved at `files/phase2-plan.md` in the session folder, and each subphase is tracked in the SQL table `p2`. When you've reviewed it, confirm D1–D6 or change them, and we'll start **N0.1**.

