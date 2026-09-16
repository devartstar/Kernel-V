# Phase 1: VFS Tree

## Goal

Build the in-kernel namespace tree:

```text
/
├── hello.txt
├── etc/
└── dev/
```

---

## Subphase 1.1: Define VFS Types

Build:

```c
vfs_node_type_t
vfs_node_ops_t
vfs_node_t
vfs_file_t
```

Learn:

```text
node = filesystem object
file = opened handle with offset
ops = backend function table
private_data = backend-owned state
```

Validation:

```text
types compile cleanly
no runtime logic yet
```

---

## Subphase 1.2: Create Root Node

Build:

```c
vfs_init();
vfs_get_root();
```

Expected:

```text
[VFS] root initialized
```

Validation:

```text
root != NULL
root->name == "/"
root->type == VFS_NODE_DIR
root->parent == NULL
```

---

## Subphase 1.3: Add Child Nodes

Build:

```c
vfs_create_node();
vfs_add_child();
```

Expected tree:

```text
/
├── hello.txt
└── dev
```

Validation:

```text
child->parent == root
root->first_child != NULL
siblings linked correctly
```

---

## Subphase 1.4: Lookup One Path Component

Build:

```c
vfs_find_child(parent, "name");
```

Validation:

```text
find "hello.txt" succeeds
find "missing" fails
```

---

## Subphase 1.5: Absolute Path Lookup

Build:

```c
vfs_lookup_absolute("/hello.txt");
vfs_lookup_absolute("/dev/null");
```

Support only:

```text
absolute paths
single slash separators
no "." or ".."
no relative paths
```

Validation:

```text
lookup "/" succeeds
lookup "/hello.txt" succeeds
lookup "/dev" succeeds
lookup "/missing" fails
```

---

## Phase 1 Completion Criteria

Phase 1 is complete when:

```text
VFS root exists
nodes can be inserted
children can be searched
absolute path lookup works
invalid paths fail safely
```
```

