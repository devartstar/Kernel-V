#include "fs/vfs.h"
#include "fs/vfs_utils.h"
#include "lib/printk.h"
#include "mm/pool_alloc.h"
#include "stddef.h"

vfs_node_t g_vfs_root;
vfs_node_t *g_vfs_root_ptr = NULL;
vfs_node_t g_vfs_nodes[VFS_MAX_NODES];
uint32_t g_vfs_nodes_count = 0;

static pool_allocator_t vfs_file_pool;

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

vfs_node_t *vfs_lookup_absolute(const char *path) {
    const char *p;
    char component[VFS_NAME_MAX];
    uint32_t i;

    vfs_node_t *start_node;
    vfs_node_t *curr_node;
    vfs_node_t *next_node;

    /* check for valid path input for lookup */
    if (!path) {
        KLOG_ERROR("VFS", "lookup failed. path for lookup is NULL.\n");
        return NULL;
    }

    /* check if the root is initialized */
    if (!g_vfs_root_ptr) {
        KLOG_ERROR("VFS", "lookup failed. root node is not initialized.\n");
        return NULL;
    }

    /* absolute path should always start from the root */
    if (path[0] != '/') {
        KLOG_ERROR("VFS",
                   "lookup failed. absoulte path %s for lookup doesn't start "
                   "with /(root).\n",
                   path);
        return NULL;
    }

    /* handle case where absolute path is empty */
    if (path[0] == '\0') {
        KLOG_VERBOSE("VFS",
                     "lookup completed. ablsolute path %s for lookup is empty "
                     "returning root.\n",
                     path);
        return g_vfs_root_ptr;
    }

    /* start seaching in the vfs tree */
    start_node = g_vfs_root_ptr;
    curr_node = start_node;

    /* always start lookup for next node from name and not separator */
    p = path + 1;

    /* iterate until entire path is traversed */
    while (*p) {
        i = 0;

        /* extract the string upto next separator in component */
        while (*p && *p != '/') {
            if (i >= VFS_NAME_MAX - 1) {
                KLOG_ERROR(
                    "VFS",
                    "lookup failed. parsed path upto maximum length %u.\n",
                    VFS_NAME_MAX);
                return NULL;
            }
            component[i++] = *p;
            p++;
        }
        component[i] = '\0';
        KLOG_VERBOSE("VFS",
                     "Looking up component %s in the absolute path %s.\n",
                     component, path);

        /* lookup for the component for child node under the current node as
         * parent */
        next_node = vfs_find_child(curr_node, component);
        if (!next_node) {
            KLOG_ERROR(
                "VFS",
                "lookup failed. child %s is not present under parent %s.\n",
                component, curr_node->name);
            return NULL;
        }

        /* always start lookup for next node from name and not separator */
        if (*p == '/') {
            p++;

            /* check if we reached the end of the path */
            if (*p == '\0') {
                KLOG_ERROR("VFS",
                           "lookup failed. parsed path %s till the end.\n",
                           path);
                return NULL;
            }

            /* procced for next iteration only if currnet node is directory */
            if (curr_node->type != VFS_NODE_DIR) {
                KLOG_ERROR(
                    "VFS",
                    "lookup failed. stoping lookup, current node %s type "
                    "%s, expected DIRECTORY to continue.\n",
                    curr_node->name, vfs_get_node_type(curr_node->type));
                return NULL;
            }
        }

        curr_node = next_node;
    }

    KLOG_INFO("VFS",
              "lookup completed. found vfs node %s (type=%s) for path %s.\n",
              curr_node->name, vfs_get_node_type(curr_node->type), path);
    return curr_node;
}

void vfs_system_init() {
    if (pool_init(&vfs_file_pool, sizeof(vfs_file_t)) < 0) {
        KLOG_ERROR("FD", "failed to intialize vfs_file pool.\n");
        return;
    }

    KLOG_INFO("FD", "vfs_file_pool: memory pool for vfs_file_t successfully "
                    "initialized.\n");
}

vfs_file_t *vfs_file_alloc() {
    vfs_file_t *file;

    file = (vfs_file_t *)pool_alloc(&vfs_file_pool);
    if (!file) {
        KLOG_ERROR("FD", "Failed to allocate memory for file object.\n");
        return NULL;
    }

    /* zero out all the bytes of the allocated memory */
    memset(file, 0, sizeof(vfs_file_t));

    file->refcount = 1;

    return file;
}

int vfs_file_free(vfs_file_t *file) {
    if (!file) {
        KLOG_ERROR("FD", "file ref to free is NULL.\n");
        return VFS_ERR_INVALID;
    }

    memset(file, 0, sizeof(vfs_file_t));
    pool_free(&vfs_file_pool, file);

    return VFS_OK;
}
