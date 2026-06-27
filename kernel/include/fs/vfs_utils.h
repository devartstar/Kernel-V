#ifndef VFS_UTILS_H
#define VFS_UTILS_H

#include "fs/vfs.h"

/**
 * vfs_get_status_string - converts the enum vfs error codes to string.
 * @status - the status macro, negative for error status, positive for success
 *
 * @returns - the status string
 */
const char *vfs_get_status_string(int status);

/**
 * vfs_get_root - get the ref. of the root node object
 *
 * @return vfs_node* - ref. of the root node object
 */
vfs_node_t *vfs_get_root(void);

/**
 * vfs_get_node_type - convert enum to string
 *
 * @type - vfs node object type
 * @return - string of the node object type */
const char *vfs_get_node_type(vfs_node_type_t type);

/**
 * vfs_create_node - create a vfs node object
 *
 * @name - reference name of the node object
 * @type - node object type
 * @ops - ref. to the operations the node object supports
 * @private_data - ref. to the backend data
 *
 * @return vfs_node_t* - ref. to the created node object/
 */
vfs_node_t *vfs_create_node(const char *name, vfs_node_type_t type,
                            const vfs_node_ops_t *ops, void *private_data);

/**
 * vfs_add_child - link child with provided parent in the vfs tree
 *
 * @parent - ref. to the parent node object existing in the tree.
 * @child - ref. to the curret object to be added under the parent.
 *
 * @return < 0 for failure and >= 0 for success.
 */
int vfs_add_child(vfs_node_t *parent, vfs_node_t *child);

/**
 * vfs_find_child - return a vfs node under the parent which matches with name
 *
 * @parent - ref. to the parent node under which child is to be found.
 * @name - name of the child to find. nexted search not supported.
 * correct name: hello.txt, dev
 * wrong name: /hello.txt, /dev/null
 *
 * @return ref. to the vfd node object if found else NULL.
 */
vfs_node_t *vfs_find_child(vfs_node_t *parent, const char *name);

#endif /* VFS_UTILS_H */
