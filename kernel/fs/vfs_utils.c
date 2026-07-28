#include "fs/vfs_utils.h"
#include "lib/printk.h"
#include "lib/string.h"

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

    /* child has an existing parent. dont allow attach twice */
    if (child->parent) {
        KLOG_ERROR("VFS", "add child failed. existing parent = %s.\n",
                   child->parent->name);
        return VFS_ERR_INVALID;
    }

    /* check if the child with same name already exist under parent.
     * this ensures unique entries in a directory */
    vfs_node_t *curr = parent->first_child;
    while (curr) {
        if (strcmp(child->name, curr->name) == 0) {
            KLOG_ERROR(
                "VFS",
                "child with same name %s (%s) exists under paretn %s (%s).\n",
                curr->name, vfs_get_node_type(curr->type), parent->name,
                vfs_get_node_type(parent->type));
            return VFS_ERR_EXISTS;
        }
        curr = curr->next_sibling;
    }

    /* link the child under the parent */
    child->parent = parent;
    child->next_sibling = parent->first_child;
    parent->first_child = child;

    KLOG_INFO("VFS", "add child success. child = %s, parent = %s.\n",
              child->name, parent->name);

    return VFS_OK;
}

vfs_node_t *vfs_find_child(vfs_node_t *parent, const char *name) {
    /* check for the validity of parent node to seach */
    if (!parent) {
        KLOG_ERROR("VFS",
                   "find child node failed. parent node to find in is NULL.\n");
        return NULL;
    }

    /* the parent node should be of type dir. to search in. */
    if (parent->type != VFS_NODE_DIR) {
        KLOG_ERROR("VFS",
                   "find child node failed. parent %s is of type %s, expected "
                   "= DIRECTORY.\n",
                   parent->name, vfs_get_node_type(parent->type));
        return NULL;
    }

    /* chech for validity of the name to search */
    if (!name) {
        KLOG_ERROR("VFS", "find child failed. name to find is NULL.\n");
        return NULL;
    }

    vfs_node_t *start_node, *curr_node, *next_node;

    /* start from the first entry under the parent dir. */
    start_node = parent->first_child;
    curr_node = start_node;

    /* keep iterating until exhausted all nodes under the dir. */
    while (curr_node != NULL) {
        if (strcmp(curr_node->name, name) == 0) {
            return curr_node;
        }
        next_node = curr_node->next_sibling;
        curr_node = next_node;
    }

    KLOG_ERROR("VFS", "find child failed. no child %s under parent %s.\n", name,
               parent->name);
    return NULL;
}

const char *vfs_get_status_string(int status) {
    switch (status) {
    case VFS_OK:
        return "OK";
    case VFS_ERR_INVALID:
        return "ERR_INVALID";
    case VFS_ERR_NOTFOUND:
        return "ERR_NOTFOUND";
    case VFS_ERR_NOTDIR:
        return "ERR_NOTDIR";
    case VFS_ERR_NOOP:
        return "ERR_NOOP";
    case VFS_ERR_NOMEM:
        return "ERR_NOMEM";
    case VFS_ERR_AGAIN:
        return "ERR_AGAIN";
    case VFS_ERR_EXISTS:
        return "ERR_EXISTS";
    default:
        if (status > 0) {
            return "SUCCESS";
        } else {
            return "UNDEFINED_ERR";
        }
    }
}
