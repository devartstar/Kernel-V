#ifndef VFS_H
#define VFS_H

/**
 * @vfs.h
 * kernel uses a virtual file system abstraction to define common
 * filesystem objects It hides the complexity of underlying filesystem specific
 * implementation.
 *
 * this header provides:
 * vfs_node - vfs node object
 * vfs_file - vfs file object
 */

#include "stdint.h"

/* define VFS status macros, < 0 means failure, >= 0 means success/counts */
#define VFS_OK 0            /* operation succeeded */
#define VFS_ERR_INVALID -1  /* caller passed invalid inputs */
#define VFS_ERR_NOTFOUND -2 /* requested object not found */
#define VFS_ERR_NOTDIR -3   /* expected a dir but not a dir */
#define VFS_ERR_NOOP -4     /* request operation not supported */
#define VFS_ERR_NOMEM -5    /* kernel allocation failed */

/* define maximum number of node objects */
#define VFS_MAX_NODES 64

/* maximum length for the vfs node object */
#define VFS_NAME_MAX 256

/* root character */
#define VFS_ROOT_SYMBOL '/'
#define VFS_NAME_SEPARATOR VFS_ROOT_SYMBOL

typedef struct vfs_node vfs_node_t;
typedef struct vfs_file vfs_file_t;

typedef enum vfs_node_type {
    VFS_NODE_FILE = 1,
    VFS_NODE_DIR,
    VFS_NODE_CHARDEV,
} vfs_node_type_t;

/**
 * vfs_node_ops - function table for backend behaviour
 * defines the operations that can be performed on the underlying node object
 * APIs exposed to be used by the VFS layer to call underlying layer.
 *
 * @open - ref to the open opration callback
 * @read - ref to the read operation callback
 * @write - ref to the write operation callback
 */
typedef struct vfs_node_ops {
    int (*open)(vfs_node_t *node, uint32_t flags);

    /* read operation:
     * @node - ref. to the node object of dir/file to read from.
     * @offset - of the file to read
     * @buf - ref. to the buffer to store the read data
     * @len - lengh the bytes to read
     */
    int (*read)(vfs_node_t *node, uint32_t offset, void *buf, uint32_t len);

    /* write operation:
     * @node - ref. to the node object of the dir/file to write to.
     * @offset - of the file to write to.
     * @buf - ref. to the buffer contining the write contents.
     * @len - length of the contents to write.
     */
    int (*write)(vfs_node_t *node, uint32_t offset, const void *buf,
                 uint32_t len);
} vfs_node_ops_t;

/**
 * vfs_node - a node object in the filesystem tree.
 * simplified version of an inode.
 *
 * @name local name of a dir/file
 * @type node type - dir/file/chardevice
 * @size - byte size for file
 * @refcount - lifetime reference of this node
 * @ops - ref. to backend behaviour table
 * @private_data - ref. to backend owned state
 * @parent - parent node object
 * @first_child - parent nodes first child
 * @next_sibling - next entry to current parent node
 */
struct vfs_node {
    /* metadata of the node */
    const char *name;
    vfs_node_type_t type;
    uint32_t size;
    uint32_t refcount;

    /* ref to the backend data */
    vfs_node_ops_t *ops;
    void *private_data;

    /* links of the node in the tree */
    vfs_node_t *parent;
    vfs_node_t *first_child;
    vfs_node_t *next_sibling;
};

/**
 * vfs_file - opened instance of a vfs node object. Each instance of vfs_file
 * represents an opened instance of a vfs_node.
 *
 * @node - ref. to the node object opened
 * @flags - read/write mode of operations allowed on file.
 * @offset - current file offser for any operation.
 * @refcount - reference to a file local to a process
 */
struct vfs_file {
    vfs_node_t *node;

    uint32_t flags;
    uint32_t offset;
    uint32_t refcount;
};

/* root object of the vfs tree */
extern vfs_node_t g_vfs_root;
extern vfs_node_t *g_vfs_root_ptr;

/* tree of vfs node objects stored in form of array */
extern vfs_node_t g_vfs_nodes[VFS_MAX_NODES];
extern uint32_t g_vfs_nodes_count;

/**
 * vfs_init - initializes the vfs with a root node
 */
int vfs_init(void);

/**
 * vfs_lookup_absolute - search for the absolute path in a vfs tree
 * recurse thru the vfs tree one dir at a time based on the path.
 *
 * @path - absolute path string to find
 */
vfs_node_t *vfs_lookup_absolute(const char *path);

#endif /* VFS_H */
