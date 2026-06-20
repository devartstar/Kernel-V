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

/**
 * devfs_create_null - create a null character device /dev/null.
 *  add read/write operation to null device.
 *  read will do not read any private data but just return 0
 *  write will do not write to te private data just return len
 *
 * @return ref. to the null character device.
 */
vfs_node_t *devfs_create_null(void);

/**
 * devfs_create_zero - create a zero character device /dev/zero.
 *  add read/write operation to zero device.
 *  read will return all 0 charaters for the requested length
 *  write will do not write to te private data just return len
 *
 * @return ref. to the zero character device.
 */
vfs_node_t *devfs_create_zero(void);

/**
 * devfs_seed_root - create a vfs node /dev under root as the base for all
 * device files syste.
 * Add null and zero device to the devfs root
 *
 * @return VFS error/success code
 */
int devfs_seed_root(void);

#endif /* DEVFS_H */
