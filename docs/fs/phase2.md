# Phase 2: RAMFS Backend

Phase 1 gave us a namespace tree: nodes exist, can be attached, and can be found by absolute path. But nodes still do not contain file data. 
Phase 2 adds the first backend: RAMFS, where regular file bytes live in kernel memory.

- `vfs_node_t node` - files identity and namespace.
- RAMFS owns the actual bytes through `node->private_data`
- vfs does not store bytes because differen FS underneat store bytes differnetly.
- RAMFS stroes the bytes in the RAM. othef FS like etx2 stores in disk blocks.

## Subphase 2.1: RAMFS file data model
Define `ramfs_file_t`.
Attach RAMFS file data to `vfs_node_t.private_data`.

## Subphase 2.2: RAMFS read operation
Implement bounded reads from memory.
Handle EOF and partial reads.

## Subphase 2.3: RAMFS write operation
Implement bounded writes into fixed-capacity RAM files.
Update file size correctly.

## Subphase 2.4: RAMFS node creation helper
Create `ramfs_create_file(name, data, size, capacity)`.

## Subphase 2.5: Seed initial filesystem
Create:
`/hello.txt`
`/etc/banner`

## Phase 2 completion
`vfs_lookup_absolute("/hello.txt")` returns a file node whose RAMFS backend can return bytes.
