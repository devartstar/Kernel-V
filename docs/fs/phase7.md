# Phase 7 — Real `stdin` via Console Input

## Goal

Phase 6 made standard descriptors real:

```text
fd 0 -> /dev/stdin
fd 1 -> /dev/stdout
fd 2 -> /dev/stderr
```

But currently:

```text
read(0, buf, len) -> VFS_ERR_NOOP
```

Phase 7 makes `fd 0` useful.

Target path:

```text
user types input
    ↓
serial / keyboard input driver
    ↓
console input buffer
    ↓
/dev/stdin read op
    ↓
fd_read(current_proc, 0, ...)
    ↓
SYS_READ copies data to user buffer
```

For your current QEMU/headless setup, the first backend should be **serial-backed stdin**, because you already use serial output and likely run with `-nographic` / `-serial stdio`.

---

# Phase 7.1 — Define the input architecture

Do not make `/dev/stdin` talk directly to UART or keyboard hardware. Put a small input layer in between.

Architecture:

```text
UART / keyboard interrupt
    ↓
console_input_push(char c)
    ↓
global console input ring buffer
    ↓
devstdin_read()
    ↓
fd_read()
    ↓
SYS_READ
    ↓
user buffer
```

This gives you a clean separation:

```text
serial.c / keyboard.c  -> hardware input producer
console_input.c        -> generic console input buffer
devfs.c                -> VFS device endpoint
syscall.c              -> user/kernel copy boundary
```

The key design rule:

```text
/dev/stdin should consume buffered input.
It should not know which hardware device produced the bytes.
```

Later, both UART and keyboard can feed the same console input buffer.

---

# Phase 7.2 — Add a console input ring buffer

Create a small byte ring buffer for input.

Conceptual state:

```text
buffer[N]
head = next write position
tail = next read position
count = number of buffered bytes
```

Required operations:

```text
console_input_init()
console_input_push(char c)
console_input_pop(char *out)
console_input_available()
console_input_read(buf, len)
```

Behavior:

```text
push:
    called by serial/keyboard input path
    inserts one byte if space exists

pop/read:
    called by /dev/stdin read op
    removes bytes from the buffer
```

Overflow policy for FS v0:

```text
if input buffer is full, drop the newest character
```

That is simpler than overwriting unread input.

Important later improvement:

```text
protect ring buffer operations with interrupt disable/restore
```

Because producer may run from interrupt context and consumer may run from syscall context.

---

# Phase 7.3 — Add a “no data available” error

`VFS_ERR_NOOP` no longer fits once stdin has a real read operation.

After Phase 7:

```text
read op missing       -> VFS_ERR_NOOP
stdin exists but empty -> VFS_ERR_AGAIN
stdin has data         -> positive byte count
```

Add a new error:

```text
VFS_ERR_AGAIN
```

Meaning:

```text
operation would block / no data currently available
```

Why not return `0`?

Because for normal files:

```text
read returns 0 -> EOF
```

But stdin is not EOF. It is just empty right now. Until you implement blocking reads, `VFS_ERR_AGAIN` is clearer.

Target behavior:

```text
read(0, buf, 8) with no input -> VFS_ERR_AGAIN
read(0, buf, 8) with input    -> number of bytes read
```

---

# Phase 7.4 — Add serial input support

Since you already have serial output, add minimal serial receive support.

Conceptual UART facts:

```text
COM1 data port        = 0x3F8
line status register  = 0x3F8 + 5
LSR bit 0             = data ready
```

Minimal polling helper:

```text
serial_has_data()
serial_read_char()
```

Then there are two possible implementation paths.

## Option A — Polling first

`devstdin_read()` checks the UART for any pending bytes and pushes them into the input ring before reading from the ring.

Flow:

```text
read(0, ...)
    ↓
serial_drain_input_to_console_buffer()
    ↓
console_input_read(...)
```

This is easiest and avoids IRQ complexity.

Downside:

```text
input is collected only when read() is called
```

That is acceptable for Phase 7 bring-up.

## Option B — Interrupt-driven UART input

UART receive interrupt fires when a byte arrives:

```text
UART IRQ
    ↓
serial interrupt handler
    ↓
read received byte
    ↓
console_input_push(byte)
```

This is more OS-correct, but requires stable IRQ registration, PIC unmasking, and serial interrupt enable bits.

Recommended path:

```text
Phase 7.4A: polling serial stdin
Phase 7.4B: interrupt-driven serial stdin
```

Do polling first. It gives you quick validation through user-space `read(0, ...)`.

---

# Phase 7.5 — Update `/dev/stdin` read op

Current behavior:

```text
/dev/stdin read -> VFS_ERR_NOOP
```

New behavior:

```text
/dev/stdin read:
    if len == 0:
        return 0

    optionally drain serial input into console buffer

    n = console_input_read(buf, len)

    if n == 0:
        return VFS_ERR_AGAIN

    return n
```

Important invariant:

```text
/dev/stdin read receives a kernel buffer from fd_read().
It must not receive a raw user pointer.
```

That already matches your Phase 5 syscall design.

---

# Phase 7.6 — Decide raw mode first

Do **raw byte input** first.

That means if the user types:

```text
abc
```

then reads can return:

```text
a
b
c
```

or all three bytes depending on buffer state and requested length.

Do not implement these yet:

```text
line buffering
backspace editing
Ctrl-C
terminal echo policy
canonical mode
process groups
foreground terminal ownership
```

Those are real TTY features, not needed for the first stdin implementation.

For Phase 7, stdin is just a byte stream.

---

# Phase 7.7 — Optional echo behavior

When a character is received, you can echo it to console output:

```text
typed char -> console_input_push(c) -> console_write(c)
```

But keep this optional.

Recommended default:

```text
no automatic echo inside input buffer
```

Why?

Because input buffering and output rendering are separate responsibilities. Later, a TTY line discipline can decide whether `ECHO` is enabled.

For debugging, temporary echo is acceptable, but keep it clearly marked.

---

# Phase 7.8 — User-space stdin test

Add a user process test:

```text
write(1, "type something: ", ...)
read(0, buf, 8)
write(1, "got: ", ...)
write(1, buf, n)
```

Expected behavior:

```text
If no input:
    read returns VFS_ERR_AGAIN

If input exists:
    read returns positive byte count
    stdout prints the received bytes
```

A robust first test:

```text
1. print prompt
2. loop until read(0) returns > 0
3. print received bytes
4. exit
```

This avoids failing immediately when no input is buffered.

---

# Phase 7.9 — Integration with scheduler behavior

For now, do not block the process inside `read(0)`.

Use non-blocking behavior:

```text
no input -> VFS_ERR_AGAIN
```

User program can retry:

```text
while ((n = read(0, buf, len)) == VFS_ERR_AGAIN) {
    yield();
}
```

Later, you can implement true blocking:

```text
read(0)
    ↓
if no input:
    mark process BLOCKED
    put process on stdin wait queue
    schedule another process
    wake when input arrives
```

That should be a later phase because it requires wait queues and process wakeups.

---

# Phase 7.10 — Tests and success criteria

Kernel tests:

```text
console_input_init() creates empty buffer
console_input_push('A') succeeds
console_input_read(buf, 1) returns 1
read byte is 'A'
empty read returns 0 internally
overflow does not corrupt buffer state
```

DEVFS tests:

```text
/dev/stdin exists
/dev/stdin has read op
/dev/stdin read returns VFS_ERR_AGAIN when empty
/dev/stdin read returns bytes when console input buffer has data
```

User tests:

```text
write(1, "stdin test\n", ...)
read(0, buf, len)
write(1, buf, n)
```

Final Phase 7 success criteria:

```text
1. fd 0 remains a normal VFS-backed descriptor.
2. SYS_READ has no fd == 0 special case.
3. /dev/stdin read op is reached through fd_read().
4. stdin returns VFS_ERR_AGAIN when no input exists.
5. serial input can populate the console input buffer.
6. user process can read typed bytes from fd 0.
7. stdout still works through /dev/stdout.
```

---

# Out of scope for Phase 7

Do not add these yet:

```text
full TTY subsystem
canonical line discipline
backspace editing
terminal modes
Ctrl-C / signals
blocking wait queues
poll/select
pipes
dup/dup2
fork fd inheritance
keyboard scancode decoding, unless you choose keyboard instead of serial
```

Those belong to later phases.

The clean Phase 7 outcome is:

```text
stdin becomes a real byte stream backed by a console input buffer.
```

After Phase 7, the natural next phase is either:

```text
Phase 8A — blocking I/O and wait queues
Phase 8B — dup/dup2 and fd inheritance
Phase 8C — keyboard-backed console input
```
