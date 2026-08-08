# Phase 9 — TTY layer plan

Your current stack is:

```text
serial IRQ / polling
  -> console_input_push()
  -> console_input_read()
  -> /dev/stdin
  -> fd 0
  -> SYS_READ

/dev/stdout or /dev/stderr
  -> devconsole_write()
  -> vga_write() + serial_write()
```

That works, but it is still a **console input buffer**, not a TTY. Phase 9 introduces a real terminal object between hardware and `/dev/stdin/stdout/stderr`.

Target architecture:

```text
UART / keyboard driver
        ↓
tty_receive_char()
        ↓
TTY line discipline
        ↓
tty_read()
        ↓
/dev/stdin or /dev/tty0
        ↓
read(0, ...)
```

For output:

```text
write(1, ...)
        ↓
/dev/stdout
        ↓
tty_write()
        ↓
VGA + serial
```

The TTY becomes the owner of terminal behavior: echo, raw/canonical mode, backspace, line buffering, and later Ctrl-C.

---

# Phase 9.0 — Define the boundary

Do **not** delete `console_input` yet. Treat it as the prototype that Phase 9 will replace.

Current responsibility:

```text
console_input.c:
    byte ring buffer
    wait for input
    wake readers
```

New responsibility:

```text
tty.c:
    byte input buffer
    wait for input
    wake readers
    optional echo
    future line editing
    future canonical/raw modes
```

So Phase 9 is not about changing syscalls. `SYS_READ` and `SYS_WRITE` should stay generic.

Correct layering after Phase 9:

```text
syscall.c  -> user/kernel copy only
fd.c       -> fd resolution and offset/refcount
devfs.c    -> maps /dev/stdin/stdout/stderr to TTY ops
tty.c      -> terminal semantics
serial.c   -> UART hardware
vga.c      -> raw video output
```

---

# Phase 9.1 — Add minimal `tty_t`

Create:

```text
kernel/include/drivers/tty.h
kernel/drivers/tty/tty.c
```

Start with exactly one terminal:

```text
g_tty0
```

Minimal `tty_t` fields:

```text
input buffer
head
tail
count
lock
echo_enabled
```

Conceptual structure:

```c
typedef struct tty {
    char input_buf[TTY_INPUT_BUF_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;

    uint8_t echo_enabled;

    spinlock_t lock;
} tty_t;
```

Functions:

```text
tty_init()
tty_get_default()
tty_receive_char(tty, char)
tty_read(tty, buf, len)
tty_write(tty, buf, len)
tty_available(tty)
```

In this subphase, `tty_receive_char()` should behave like your current `console_input_push()`:

```text
insert byte into tty input buffer
wake process waiting for console/tty input
```

`tty_read()` should behave like your current `console_input_read()`:

```text
read available bytes from tty input buffer
return byte count
```

Do not add backspace, line mode, Ctrl-C, or canonical mode yet.

---

# Phase 9.2 — Move stdin read path to TTY

Current `/dev/stdin`:

```text
devstdin_read()
    -> console_input_wait_for_data()
    -> console_input_read()
```

New `/dev/stdin`:

```text
devstdin_read()
    -> tty_wait_for_data(g_tty0)
    -> tty_read(g_tty0, buf, len)
```

For now, you can keep the same wait reason:

```text
PROC_WAIT_CONSOLE_INPUT
```

But rename later to:

```text
PROC_WAIT_TTY_INPUT
```

Recommended clean rename in Phase 9:

```c
PROC_WAIT_TTY_INPUT
```

This is more accurate because the process is not waiting on the generic console anymore. It is waiting on a terminal input queue.

---

# Phase 9.3 — Move serial receive path to TTY

Current serial input path:

```text
serial_irq_handler()
    -> serial_drain_input()
    -> console_input_push(c)
```

New serial input path:

```text
serial_irq_handler()
    -> serial_drain_input()
    -> tty_receive_char(g_tty0, c)
```

This is the most important architectural shift.

The serial driver should not know about `/dev/stdin`. It should only feed received bytes into the active terminal.

Later, keyboard input will do the same:

```text
keyboard_irq_handler()
    -> decode scancode
    -> tty_receive_char(g_tty0, c)
```

---

# Phase 9.4 — Move stdout/stderr write path to TTY

Current `/dev/stdout`:

```text
devconsole_write()
    -> vga_write()
    -> serial_write()
```

New `/dev/stdout`:

```text
devstdout_write()
    -> tty_write(g_tty0, buf, len)
```

Then:

```text
tty_write()
    -> vga_write()
    -> serial_write()
```

This makes terminal output symmetrical with terminal input.

The device nodes become thin wrappers:

```text
/dev/stdin   -> tty_read(g_tty0)
/dev/stdout  -> tty_write(g_tty0)
/dev/stderr  -> tty_write(g_tty0)
/dev/console -> tty_read/write(g_tty0)
```

---

# Phase 9.5 — Add echo mode

Once input flows through TTY, echo becomes simple.

When serial receives a character:

```text
tty_receive_char(c)
    -> store c in input buffer
    -> if echo_enabled:
           tty_write(c)
    -> wake reader
```

For now:

```text
echo_enabled = 1
```

That gives normal terminal behavior:

```text
user types 'a'
screen shows 'a'
read(0) later returns 'a'
```

But keep echo controlled by the TTY layer, not serial and not DEVFS.

Wrong:

```text
serial driver echoes character
```

Correct:

```text
TTY line discipline echoes character
```

---

# Phase 9.6 — Add minimal canonical mode

Raw mode returns bytes as soon as available.

Canonical mode waits until newline.

Raw mode:

```text
type: a
read(0) can return "a"
```

Canonical mode:

```text
type: a b c Enter
read(0) returns "abc\n"
```

For first canonical implementation:

```text
buffer input normally
read blocks until '\n' exists
then returns up to and including newline
```

Do not add full POSIX terminal behavior yet. Just line buffering.

---

# Phase 9.7 — Add backspace handling

Once canonical mode exists, add backspace.

Input behavior:

```text
type 'a' -> buffer: a
type 'b' -> buffer: ab
type Backspace -> buffer: a
```

Echo behavior:

```text
Backspace echo sequence:
    '\b'
    ' '
    '\b'
```

This visibly erases one character on the screen.

Backspace belongs in TTY, not in serial, because serial only sees bytes; TTY understands editing semantics.

---

# Phase 9.8 — Add `/dev/tty0`

Currently you have:

```text
/dev/stdin
/dev/stdout
/dev/stderr
/dev/console
```

Add:

```text
/dev/tty0
```

Then eventually stdio can be installed as:

```text
fd 0 -> /dev/tty0
fd 1 -> /dev/tty0
fd 2 -> /dev/tty0
```

For now, you may keep:

```text
fd 0 -> /dev/stdin
fd 1 -> /dev/stdout
fd 2 -> /dev/stderr
```

but internally all three should call the same `g_tty0`.

---

# Phase 9.9 — Tests

Kernel tests:

```text
tty_init() creates empty tty
tty_receive_char('A') stores byte
tty_read(buf, 1) returns 'A'
tty_read() blocks if empty
tty_receive_char() wakes blocked reader
tty_write("abc", 3) prints exactly 3 bytes
```

User tests:

```text
write(1, "type line: ", 11)
read(0, buf, sizeof(buf))
write(1, "got: ", 5)
write(1, buf, n)
```

For raw mode:

```text
typing one byte should wake read
```

For canonical mode:

```text
typing does not complete read until Enter
```

---

# Phase 9 success criteria

Phase 9 is complete when:

```text
1. serial input feeds tty_receive_char(), not console_input_push().
2. /dev/stdin reads from tty_read().
3. /dev/stdout and /dev/stderr write through tty_write().
4. TTY can optionally echo received input.
5. Blocking read still works.
6. Raw mode works.
7. Minimal canonical newline mode works.
8. Backspace works in canonical mode.
9. SYS_READ and SYS_WRITE remain generic.
```

---
