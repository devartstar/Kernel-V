/**

                           ┌───────────────┐
                           │PROCESS        │
                          ┌┴───────────────┴────┐
                          │                     │
┌─────────────┐           │                     │
│ UART IRQ4   │   ┌───────▼───────┐             │
└──────┬──────┘   │/dev/stdin     │      ┌──────▼───────┐
       │          │READ           │      │Console Write │
       │          └───────┬───────┘      └──────┬───────┘
┌──────▼──────┐           │                     │
│ TTY RX PORT │   ┌───────▼───────┐             │
└──────┬──────┘   │tty_read       │      ┌──────▼───────┐
       │          │ (session)     │      │tty_write     │
       │          └───────┬───────┘      │  (session)   │
┌──────▼───────┐          │              └──────┬───────┘
│ dedicated    │  ┌───────▼───────┐             │
│ INPUT CHANNEL│  │NO readable    │             │
└──────┬───────┘  │ bytes - WAIT  │      ┌──────▼───────┐
       │          └───────┬───────┘      │Backend Port  │
       │                  │              └──────┬───────┘
┌──────▼────────┐ ┌───────▼───────────┐         │
│CHANNEL HANDLES│ │Bytes Available    │  ┌──────▼───────┐
│1. STAGE       │ │Wakeup - READ      │  │Serial Driver │
│2. COMMIT      │ │under lock with    │  │    OR        │
│3. READ        │ │interrupts disabled│  │VGA Driver    │
└───────────────┘ └───────────────────┘  └──────────────┘

*/

/**
 * channel => storage + ordering + sync (only thing that hold bytes)
 * session => owner of the two channels + policy. Owns the buffers
 * port => stateless bridge to hardware. doesnt hold any byte.
                       ┌──────────────────────────────────────────┐
                       │            tty_session_t                 │
                       │  (the "TTY" — owns semantics & storage)  │
                       │                                          │
   process side        │   ┌───────────────┐   in_buf[256]        │
  read()  ◄────────────┼───┤ input  chan   │──► [ring storage]    │
                       │   │ r ≤ c ≤ w     │                      │
                       │   └───────▲───────┘                      │
                       │           │ stage/commit                 │
                       │           │                              │
  write() ────────────►│   ┌───────┴───────┐   out_buf[256]       │
                       │   │ output chan   │──► [ring storage]    │
                       │   │ r ≤ c ≤ w     │                      │
                       │   └───────┬───────┘                      │
                       │           │ port->ops->putc(byte)        │
                       │      ┌────▼─────┐                        │
                       │      │  port*   │──┐                     │
                       └──────┴──────────┴──┼─────────────────────┘
                                  ▲         │ back-ref
                                  │         ▼
                            ┌─────┴────────────────────┐
                            │      tty_port_t          │  (the backend)
                            │  ops->putc  ──► VGA/UART │
                            │  session*   ──► back up  │
                            └──────────▲───────────────┘
                                       │ tty_port_rx(port, byte)
                                  ┌────┴─────┐
                                  │ UART IRQ │  hardware
                                  └──────────┘
*/
