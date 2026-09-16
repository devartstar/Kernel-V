## Phase 7 — Raw serial-backed stdin

Goal:

```text
typed serial input
  -> console_input buffer
  -> /dev/stdin
  -> fd 0
  -> SYS_READ
  -> user buffer
```

Subphases:

```text
7.0  Small hardening fixes before stdin hardware input
7.1  Add serial RX polling helpers
7.2  Add serial_drain_input() into console_input
7.3  Call serial_drain_input() from devstdin_read()
7.4  Add user-mode stdin polling test
7.5  Optional temporary echo
7.6  Phase 7 cleanup and invariants
```

No blocking. No TTY. No keyboard yet.

---

## Phase 8 — Blocking read and wait queues

Goal:

```text
read(0, buf, len)
  -> if no input, block process
  -> scheduler runs another process
  -> input arrival wakes blocked reader
```

This introduces:

```text
wait queues
PROC_WAITING reason
wakeup path
blocking fd/device read semantics
```

This should come before a real TTY because canonical TTY input needs blocking behavior.

---

## Phase 9 — TTY layer

Goal:

```text
serial/keyboard driver
  -> tty input layer
  -> line discipline
  -> /dev/ttyS0 or /dev/tty0
  -> process fd 0/1/2
```

TTY will own:

```text
echo
backspace
line buffering
raw mode vs canonical mode
Ctrl-C later
/dev/tty
/dev/console semantics
```

This is where terminal behavior really begins.

---

## Phase 10 — FD duplication and redirection

Goal:

```text
dup()
dup2()
stdio redirection
shared vfs_file_t offsets
```

This lets you model:

```text
fd 1 redirected to a file
fd 2 redirected to same output
multiple fds sharing same open-file object
```

This is where `vfs_file_t.refcount` becomes much more meaningful.

---
