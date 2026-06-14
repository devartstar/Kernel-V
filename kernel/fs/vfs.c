#include "fs/vfs.h"
#include "fs/vfs_utils.h"
#include "lib/printk.h"
#include "stddef.h"

vfs_node_t g_vfs_root;
vfs_node_t *g_vfs_root_ptr = NULL;
vfs_node_t g_vfs_nodes[VFS_MAX_NODES];
uint32_t g_vfs_nodes_count = 0;

int vfs_init(void) {
    g_vfs_root.name = "/";
    g_vfs_root.type = VFS_NODE_DIR;

    /* intializing with size 0 its an empty directory */
    g_vfs_root.size = 0;

    /* refcount assigned to 1 since root should always exists */
    g_vfs_root.refcount = 1;

    g_vfs_root.ops = NULL;
    g_vfs_root.private_data = NULL;

    /* currently only node in the tree */
    g_vfs_root.parent = NULL;
    g_vfs_root.first_child = NULL;
    g_vfs_root.next_sibling = NULL;

    /* reset transient node storage for a clean VFS state */
    g_vfs_nodes_count = 0;

    g_vfs_root_ptr = &g_vfs_root;

    KLOG_INFO("VFS", "root initialized.\n");

    return VFS_OK;
}
