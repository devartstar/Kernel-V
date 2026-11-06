Structure:
```
 Node A                                                     Node B
 ┌────────────────────────────┐                ┌────────────────────────────┐
 │ KRPC: "call fn 7 with x=5" │                │ KRPC handler runs fn 7     │
 │      (a C struct)          │                │      (a C struct)          │
 │        │ wire_wr_t         │                │        ▲ wire_rd_t         │
 │        ▼ (serialize)       │                │        │ (parse/validate)  │
 │ netbuf: [hdr][bytes....]   │                │ netbuf: [hdr][bytes....]   │
 │        │ virtio-net TX     │   Ethernet     │        ▲ virtio-net RX     │
 │        └───────────────────┼───── wire ─────┼────────┘                   │
 └────────────────────────────┘                └────────────────────────────┘
```

- netbuffer -> the storage (DMA slot) which holds the packet bytes -> passed thru the wires.
- message -> in memory structure lives in stack
- wire_rd_t -> translator - cursor for read bytes for a buffer from another machine. it link between the netbuf and message.
    A packet from another machine cannot be trusted. read should go thru the cursor only the specified length.
    ```
    netbuf->data                                             netbuf->data + len
       │                                                            │
       ▼                                                            ▼
       [ 88 B5 | 01 | 00 | 2A 00 00 00 | ...payload...              ]
                   ▲                                                ▲
                 r.p  (next byte to read; moves right)            r.end (fence)
       r.err = false   (becomes true forever after the first overrun)
    ```
- wire_wr_t -> cursor for writing position to a netbuff tail (free tailroom)
