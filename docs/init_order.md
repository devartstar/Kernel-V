```
kernel_main -> printk_init
printk_init -> serial_init & vga_init
```

### Initialization Order.

1. vfs_system_init
2. vfs_init
3. ramfs_seed_root
4. console_input_init
5. devfs_seed_root

### What each initialization does.

vfs_system_init - initialize a memory pool for vfs_file object) 
`pool_allocator_t g_vfs_file_pool`

vfs_init - initializes the global vfs root node.
root node file = '/'
```
vfs_node_t g_vfs_root
vfs_node_t *g_vfs_root_ptr
```

```
vfs_node_t g_vfs_nodes[VFS_MAX_NODES]
g_vfs_nodes_count
```

In `ramfs_seed_root()` we call `ramfs_create_file()` and it returns a vfs node
object - but there isno check if the node object is already presnet.
then in vfs_add_child to the parent, there is no checkif the child has been
added under same parent.
Are these above checks necessary in kernel or not.


```
ramfs_file_t g_ramfs_files[RAMFS_MAX_FILES]
g_ramfs_file_count
```
