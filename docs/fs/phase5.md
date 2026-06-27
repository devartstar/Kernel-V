# Phase 5 Subphases

## 5.0 — Syscall preparation and user-copy helpers

Before adding `open/read/write`, we need safe helpers for moving data between user memory and kernel memory.

You already have some user pointer validation logic in `syscall.c`. We will build on that.

We need:

```c id="7euj8s"
copy_user_string()
copy_from_user()
copy_to_user()
```

Purpose:

```text id="zhmyus"
SYS_OPEN   needs to copy path string from user to kernel
SYS_READ   needs to copy read bytes from kernel buffer to user buffer
SYS_WRITE  needs to copy write bytes from user buffer to kernel buffer
```

This prevents raw user pointers from being passed directly into RAMFS/DEVFS.

---

## 5.1 — `SYS_OPEN`

Kernel-side syscall:

```c id="e99xco"
int sys_open(const char *user_path, uint32_t flags);
```

Flow:

```text id="crdnqp"
user path pointer
    ↓
copy_user_string()
    ↓
fd_open_path(current_proc, kpath, flags)
    ↓
return fd
```

Expected behavior:

```text id="se5ahx"
open("/hello.txt", 0) → 3
open("/missing", 0)   → VFS_ERR_NOTFOUND
```

This proves that a user process can create entries in its own `current_proc->fds[]`.

---

## 5.2 — `SYS_READ`

Kernel-side syscall:

```c id="1ih1wx"
int sys_read(int fd, void *user_buf, uint32_t len);
```

Flow:

```text id="2gzuqd"
fd
    ↓
fd_read(current_proc, fd, kernel_buffer, len)
    ↓
copy_to_user(user_buf, kernel_buffer, bytes_read)
    ↓
return bytes_read
```

Important rule:

```text id="qloaqn"
RAMFS/DEVFS should receive kernel buffers, not raw user buffers.
```

Expected behavior:

```text id="gnjss4"
fd = open("/hello.txt", 0)
read(fd, buf, 5) → 5 bytes copied into user buffer
```

This proves user processes can consume filesystem data.

---

## 5.3 — `SYS_WRITE`

Kernel-side syscall:

```c id="1qkgl8"
int sys_write(int fd, const void *user_buf, uint32_t len);
```

Flow:

```text id="dkg7y8"
user buffer
    ↓
copy_from_user(kernel_buffer, user_buf, len)
    ↓
fd_write(current_proc, fd, kernel_buffer, len)
    ↓
return bytes_written
```

Expected behavior:

```text id="xviuj0"
fd = open("/hello.txt", 0)
write(fd, "ABC", 3) → 3
```

This will eventually replace or extend your current `SYS_WRITE`, which probably only handles stdout-like debug printing.

We need to decide whether:

```text id="5p82d7"
fd == 1 → console/stdout special path
fd >= 3 → VFS fd path
```

That is probably the cleanest FS v0 design.

---

## 5.4 — `SYS_CLOSE`

Kernel-side syscall:

```c id="kk3gaa"
int sys_close(int fd);
```

Flow:

```text id="d9ded0"
fd
    ↓
fd_close(current_proc, fd)
    ↓
return VFS_OK / error
```

Expected behavior:

```text id="0d83zu"
close(3)      → VFS_OK
read(3, ...)  → VFS_ERR_INVALID
close(3)      → VFS_ERR_INVALID
```

This proves fd lifecycle is visible to user mode.

---

## 5.5 — `SYS_LSEEK`

Kernel-side syscall:

```c id="o9o4ax"
int sys_lseek(int fd, int32_t offset, int whence);
```

Flow:

```text id="5w99cz"
fd + offset + whence
    ↓
fd_lseek(current_proc, fd, offset, whence)
    ↓
return new offset
```

Expected behavior:

```text id="c9ahik"
fd = open("/hello.txt", 0)
lseek(fd, 0, SEEK_SET) → 0
read(fd, buf, 5)       → first 5 bytes
lseek(fd, 0, SEEK_SET) → 0
read(fd, buf, 5)       → same first 5 bytes again
```

This proves open-file offset semantics are correct.

---

## 5.6 — User-space wrappers and integration test

After syscalls exist, add small user-side wrappers:

```c id="qkhwmb"
int open(const char *path, uint32_t flags);
int read(int fd, void *buf, uint32_t len);
int write(int fd, const void *buf, uint32_t len);
int close(int fd);
int lseek(int fd, int32_t offset, int whence);
```

Then write one user process test:

```c id="3izajr"
int fd = open("/hello.txt", 0);

char buf[16];
int n = read(fd, buf, 5);

write(1, buf, n);

lseek(fd, 0, SEEK_SET);
n = read(fd, buf, 5);

close(fd);
```

Expected end-to-end proof:

```text id="p12df5"
user process enters syscall
kernel validates user pointers
kernel uses current_proc->fds[]
VFS reaches RAMFS/DEVFS backend
data returns to user process
```

---

# Phase 5 Boundary

Phase 5 should **not** implement:

```text id="vfeygw"
create()
unlink()
mkdir()
readdir()
permissions
mount()
fork fd inheritance
dup()
```