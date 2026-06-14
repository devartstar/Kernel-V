#include "fs/vfs_utils.h"
#include "lib/printk.h"

vfs_node_t *vfs_get_root() { return g_vfs_root_ptr; }

const char *vfs_get_node_type(vfs_node_type_t type) {
    switch (type) {
    case VFS_NODE_FILE:
        return "FILE";
    case VFS_NODE_DIR:
        return "DIRECTORY";
    case VFS_NODE_CHARDEV:
        return "CHARACTER_DEVICE";
    default:
        return "INVALID_TYPE";
    }
    return "INVALID_TYPE";
}

vfs_node_t *vfs_create_node(const char *name, vfs_node_type_t type,
                            const vfs_node_ops_t *ops, void *private_data) {
    vfs_node_t *node;

    /* validate input arguments */
    if (!name) {
        KLOG_ERROR("VFS", "create vfs node failed. name is NULL.\n");
        return NULL;
    }

    /* check the count of node object in global list */
    if (g_vfs_nodes_count >= VFS_MAX_NODES) {
        KLOG_ERROR("VFS",
                   "create vfs node failed. max node count %u reached.\n",
                   g_vfs_nodes_count);
        return NULL;
    }

    /* assign global add to the node reference */
    node = &g_vfs_nodes[g_vfs_nodes_count++];

    /* assign default initialize vlaues to node object */
    node->name = name;
    node->type = type;

    node->size = 0;
    node->refcount = 1;

    node->ops = ops;
    node->private_data = private_data;

    /* linkage to other nodes of the tree will be done when added to tree */
    node->parent = NULL;
    node->first_child = NULL;
    node->next_sibling = NULL;

    return node;
}

int vfs_add_child(vfs_node_t *parent, vfs_node_t *child) {
    /* check input arguments validity */
    if (!parent || !child) {
        KLOG_ERROR("VFS",
                   "add child failed. invalid parent %p or child %p ref.\n",
                   parent, child);
        return VFS_ERR_INVALID;
    }

    /* child can only be added under a node of type DIRECTORY */
    if (parent->type != VFS_NODE_DIR) {
        KLOG_ERROR(
            "VFS",
            "add child failed. parent type = %s, expected = DIRECTORY.\n",
            vfs_get_node_type(parent->type));
        return VFS_ERR_NOTDIR;
    }

    /* link the child under the parent */
    child->parent = parent;
    child->next_sibling = parent->first_child;
    parent->first_child = child;

    KLOG_INFO("VFS", "add child success. child = %s, parent = %s.\n",
              child->name, parent->name);

    return VFS_OK;
}
