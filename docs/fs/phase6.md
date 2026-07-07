# Phase 6 — VFS-backed Standard I/O

## Goal

Phase 6 converts `stdin`, `stdout`, and `stderr` from syscall special cases into normal process file descriptors backed by VFS nodes.

Target state:

```text
fd 0 -> /dev/stdin or /dev/console
fd 1 -> /dev/stdout or /dev/console
fd 2 -> /dev/stderr or /dev/console
```

The main cleanup is to remove this kind of logic from `SYS_WRITE`:

```text
if fd == 1:
    print directly
else:
    fd_write(...)
```

After Phase 6, `SYS_WRITE` should just validate/copy the user buffer and call:

```text
fd_write(current_proc, fd, ...)
```

Console behavior should live inside the DEVFS character-device backend, not inside the syscall layer.

---

## Phase 6.1 — Choose the stdio device model

Use this design:

```text
/dev/console
/dev/stdin
/dev/stdout
/dev/stderr
```

`/dev/console` is the underlying logical console device. `/dev/stdin`, `/dev/stdout`, and `/dev/stderr` are standard stream device nodes. Internally, they can all use the same console backend for now.

This gives you a clean learning model:

```text
fd 0 -> /dev/stdin
fd 1 -> /dev/stdout
fd 2 -> /dev/stderr
```

Validation target:

```text
vfs_lookup_absolute("/dev/console") != NULL
vfs_lookup_absolute("/dev/stdin")   != NULL
vfs_lookup_absolute("/dev/stdout")  != NULL
vfs_lookup_absolute("/dev/stderr")  != NULL
```

---

## Phase 6.2 — Add a console character device backend

DEVFS currently has `/dev/null` and `/dev/zero`. Add a console-style character device.

The first useful operation is `write`.

Flow:

```text
write(1, "hello", 5)
  -> SYS_WRITE
  -> fd_write(current_proc, 1, ...)
  -> stdout node write op
  -> console/serial output
```

For now, `read` can return `VFS_ERR_NOOP` until you build keyboard/TTY input.

The important architectural change is this:

```text
syscall.c no longer knows how console output works
devfs console node knows how console output works
```

---

## Phase 6.3 — Install fd `0`, `1`, `2` during user process creation

Normal `fd_alloc()` starts at `PROCESS_FIRST_NORMAL_FD`, usually `3`. That is correct for ordinary opens.

But standard descriptors need exact slots:

```text
fd 0 = stdin
fd 1 = stdout
fd 2 = stderr
```

So add an fd helper conceptually like:

```text
fd_install(proc, exact_fd, file)
```

It should fail if:

```text
proc is NULL
file is NULL
exact_fd is outside range
proc->fds[exact_fd] is already occupied
```

Then add a process setup routine:

```text
proc_setup_stdio(proc)
```

Conceptual flow:

```text
open /dev/stdin  into fd 0
open /dev/stdout into fd 1
open /dev/stderr into fd 2
```

Validation target:

```text
proc->fds[0] != NULL
proc->fds[1] != NULL
proc->fds[2] != NULL
```

---

## Phase 6.4 — Decide where stdio initialization belongs

Do **not** blindly install stdio for every PCB unless that is your temporary simplification.

Better rule:

```text
kernel process: stdio optional
user process: stdio installed before first user instruction
```

So the best integration point is your user process creation path, not generic `proc_alloc()`.

The first user instruction should be able to do:

```text
write(1, "hello", 5)
```

without explicitly opening `/dev/stdout`.

---

## Phase 6.5 — Remove special `fd == 1` handling

Once `fd 1` is real, simplify `SYS_WRITE`.

Before:

```text
if fd == 1:
    printk(...)
else:
    fd_write(...)
```

After:

```text
copy_from_user(kbuf, user_buf, len)
fd_write(current_proc, fd, kbuf, len)
return bytes_written
```

This is the payoff of Phase 6.

The syscall layer becomes generic. DEVFS owns device-specific behavior.

Validation target:

```text
write(1, "hello\n", 6)
```

still prints, but only because:

```text
current_proc->fds[1] -> /dev/stdout -> console write op
```

---

## Phase 6.6 — Make `stderr` real

For now, `stderr` can use the same backend as `stdout`.

```text
fd 1 -> /dev/stdout -> console write
fd 2 -> /dev/stderr -> console write
```

Later you can route stderr differently, add prefixes, colors, or serial-only output. For this phase, correctness means fd `2` is real and uses the same fd/VFS/syscall path.

Validation target:

```text
write(1, "out\n", 4)
write(2, "err\n", 4)
```

Both should print through:

```text
SYS_WRITE -> fd_write -> devfs console write
```

---

## Phase 6.7 — Define temporary `stdin` behavior

`stdin` requires input buffering, which you probably do not want to implement in this phase.

For now:

```text
read(0, buf, len) -> VFS_ERR_NOOP
```

That means the device exists, but read is unsupported.

Avoid pretending EOF unless you intentionally want that behavior. `VFS_ERR_NOOP` is cleaner because it tells you the node exists but lacks read functionality.

Later, a TTY/keyboard phase can replace this with real input.

---

## Phase 6.8 — Add integration tests

Kernel-side validations:

```text
/dev/console exists
/dev/stdin exists
/dev/stdout exists
/dev/stderr exists
```

Process validations:

```text
new_user_proc->fds[0] != NULL
new_user_proc->fds[1] != NULL
new_user_proc->fds[2] != NULL
```

User-side validation:

```text
write(1, "stdout ok\n", ...)
write(2, "stderr ok\n", ...)
fd = open("/hello.txt", 0)
read(fd, buf, 5)
write(1, buf, 5)
close(fd)
```

This proves both standard descriptors and ordinary file descriptors use the same syscall/VFS path.

---

## Phase 6.9 — Cleanup invariants

After Phase 6, these should be true:

```text
All user-visible fds are process-local.
FD 0, 1, and 2 are normal entries in pcb_t->fds[].
SYS_WRITE does not know about console internals.
SYS_READ does not know about stdin internals.
DEVFS owns device behavior.
The fd layer owns offset/refcount behavior.
The process layer owns descriptor lifetime.
```

`fd_close_all(proc)` should close stdio descriptors too. Do not special-case them during process cleanup.

---

## Out of scope for Phase 6

Do not add these yet:

```text
keyboard interrupt input buffering
canonical/raw terminal modes
line discipline
process groups
terminal ownership
blocking reads
poll/select
dup/dup2
fork fd inheritance
redirection
pipes
```

Those are later phases.

Phase 6 is only about making standard descriptors real VFS-backed descriptors.

Final success criteria:

```text
1. /dev/console, /dev/stdin, /dev/stdout, /dev/stderr exist.
2. Every user process starts with fd 0, 1, and 2 installed.
3. write(1, ...) reaches console through fd_write(), not syscall special-case code.
4. write(2, ...) works through the same mechanism.
5. read(0, ...) returns a controlled unsupported-input result.
6. Ordinary file fds still start at 3.
7. Process exit closes stdio descriptors through fd_close_all().
```
