#ifndef RAMFS_H
#define RAMFS_H

#include "fs/vfs.h"
#include "stdint.h"

/**
 * ramfs_file private data stored for regular files
 *
 * @data - ref. to the kernel memory containing file data
 * @size - number of valid bytes in the file
 * @capacity - maximum writing bytes in the backend buffer
 */
typedef struct ramfs_file {
    uint8_t *data;
    uint32_t size;
    uint32_t capacity;
} ramfs_file_t;

/**
 * ramfs_read - backend operatin for RAMFS file read
 *
 * @node ref. to the vfs node object of the file to read.
 * @offset of the file to read from
 * @buf ref. to the buffer to store the read data
 * @length of the data to read
 *
 * @return the number of bytes read
 */
int ramfs_read(vfs_node_t *node, uint32_t offset, void *buf, uint32_t len);

#endif /* RAMFS_H */
