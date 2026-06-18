# Phase 3: DEVFS Minimal

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
