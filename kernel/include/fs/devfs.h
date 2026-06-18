#ifndef DEVFS_H
#define DEVFS_H

#include "fs/vfs.h"

/**
 * devfs_create_chardev - create a character device
 * @name - character device name
 * @ops - ref. to the node operations object
 * @private_data - ref. to the backed data
 *
 * @return the ref to the char device node object.
 */
vfs_node_t *devfs_create_chardev(const char *name, const vfs_node_ops_t *ops,
                                 void *private_data);

#endif /* DEVFS_H */
