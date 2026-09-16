# Phase 3: DEVFS Minimal

Both regular files and device files use the vfs node. vfs_node_t.
RAMFS regular files use ramfs_file_t. (vfs node type = FILE)
DEVFS device files use devfs_file_t. (vfs node type = CHARDEV)

## Subphase 3.1: DEVFS structure
Create `devfs.h`, `devfs.c`, and define device-node creation helpers.

## Subphase 3.2: /dev/null
Implement:
- read returns 0
- write returns len

## Subphase 3.3: /dev/zero
Implement:
- read fills buffer with zero bytes
- write returns len

## Subphase 3.4: Seed /dev tree
Create:
- /dev
- /dev/null
- /dev/zero

## Completion
`vfs_lookup_absolute("/dev/null")` and `vfs_lookup_absolute("/dev/zero")` must return character-device nodes with working read/write ops.
