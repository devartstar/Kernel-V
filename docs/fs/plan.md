# Kernel-V FS v0

## Goal

Build the first filesystem layer for Kernel-V:

```text
user syscall
   ↓
process fd table
   ↓
vfs_file_t
   ↓
vfs_node_t
   ↓
RAMFS / DEVFS backend
```

Target user flow:

```c
int fd = open("/hello.txt", O_RDONLY);
int n = read(fd, buf, len);
close(fd);
```

---

## Scope

FS v0 supports:

```text
absolute path lookup
RAM-backed regular files
/dev/null
/dev/zero
per-process file descriptor table
open/read/write/close/lseek syscalls
basic user-kernel buffer copy
```

FS v0 does not support:

```text
disk filesystem
ext2/FAT
permissions
relative paths
cwd
mkdir/unlink/rename
mount syscall
buffer cache
page cache
mmap
journaling
```

---

## Architecture

```text
sys_open/sys_read/sys_write/sys_close
        ↓
vfs_open/vfs_read/vfs_write/vfs_close
        ↓
fd_alloc/fd_get/fd_close
        ↓
vfs_file_t
        ↓
vfs_node_t + vfs_node_ops_t
        ↓
ramfs/devfs implementation
```

---

## Core Types

```c
typedef enum vfs_node_type {
    VFS_NODE_FILE,
    VFS_NODE_DIR,
    VFS_NODE_CHARDEV,
} vfs_node_type_t;

typedef struct vfs_node vfs_node_t;

typedef struct vfs_node_ops {
    int (*open)(vfs_node_t *node, uint32_t flags);
    int (*read)(vfs_node_t *node, uint32_t offset, void *buf, uint32_t len);
    int (*write)(vfs_node_t *node, uint32_t offset, const void *buf, uint32_t len);
} vfs_node_ops_t;

struct vfs_node {
    const char *name;
    vfs_node_type_t type;
    uint32_t size;
    uint32_t refcount;

    vfs_node_ops_t *ops;
    void *private_data;

    vfs_node_t *parent;
    vfs_node_t *first_child;
    vfs_node_t *next_sibling;
};

typedef struct vfs_file {
    vfs_node_t *node;
    uint32_t flags;
    uint32_t offset;
    uint32_t refcount;
} vfs_file_t;
```

---

## Phase 1: VFS Tree

Build:

```c
vfs_init();
vfs_create_node();
vfs_add_child();
vfs_lookup_absolute();
```

Validate:

```text
lookup "/" succeeds
lookup "/hello.txt" succeeds
lookup "/missing" fails
```

---

## Phase 2: RAMFS

Build:

```c
ramfs_read();
ramfs_write();
```

Create:

```text
/hello.txt
/etc/banner
```

Behavior:

```text
read past EOF returns 0
partial read near EOF returns available bytes
write advances file size if capacity permits
```

---

## Phase 3: DEVFS

Create:

```text
/dev/null
/dev/zero
```

Behavior:

```text
/dev/null read  -> 0
/dev/null write -> len

/dev/zero read  -> zero-filled buffer, len
/dev/zero write -> len
```

---

## Phase 4: File Descriptor Table

Add to process:

```c
#define PROCESS_MAX_FDS 32

vfs_file_t *fds[PROCESS_MAX_FDS];
```

Build:

```c
fd_alloc();
fd_get();
fd_close();
```

Rules:

```text
fd is process-local
fd points to vfs_file_t
vfs_file_t owns offset
close clears fd slot
invalid fd fails
```

---

## Phase 5: Kernel File API

Build:

```c
vfs_open();
vfs_read();
vfs_write();
vfs_close();
vfs_lseek();
```

Rules:

```text
read uses file->offset
successful read advances offset
write uses file->offset
successful write advances offset
lseek modifies offset
```

---

## Phase 6: Syscalls

Build:

```c
sys_open();
sys_read();
sys_write();
sys_close();
sys_lseek();
```

Also build:

```c
copy_to_user();
copy_from_user();
copy_string_from_user();
```

Rule:

```text
syscall layer copies user memory
VFS layer only touches kernel memory
```

---

## Phase 7: User Test

User program:

```c
int main(void)
{
    char buf[64];

    int fd = open("/hello.txt", O_RDONLY);
    int n = read(fd, buf, sizeof(buf) - 1);

    buf[n] = 0;

    write(1, buf, n);
    close(fd);

    return 0;
}
```

Expected logs:

```text
[VFS] open /hello.txt -> fd=3
[VFS] read fd=3 offset=0 len=63 -> n
[VFS] close fd=3 -> ok
```

---

## Validation Matrix

```text
open existing file succeeds
open missing file fails
read advances offset
lseek SEEK_SET resets offset
close releases fd
read after close fails
/dev/null write returns len
/dev/null read returns 0
/dev/zero read returns zero-filled bytes
```

---

## Completion Criteria

FS v0 is complete when a user process can:

```text
open /hello.txt
read bytes into user memory
observe correct file offset behavior
close the fd
fail correctly on read-after-close
use /dev/null and /dev/zero
```

This is the first real UNIX-style filesystem milestone for Kernel-V.

