#ifndef RAMFS_H
#define RAMFS_H

#include "fs/vfs.h"
#include "stdint.h"

#define RAMFS_MAX_FILES 64

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

/**
 * ramfs_write - backend operatin for RAMFS file write
 *
 * @node ref. to the vfs node object of the file to write.
 * @offset of the file to read from
 * @buf ref. to the buffer to store the write data
 * @length of the data to write
 *
 * @return the number of bytes writen
 */
int ramfs_write(vfs_node_t *node, uint32_t offset, const void *buf,
                uint32_t len);

/**
 * ramfs_file_create - create a file object in the ramfs filesystem. associate
 * with vfs node object as backed data.
 *
 * @name - name of the file or node object
 * @data - ref to the data to write after creating the file
 * @size - size of the file or the data to write
 * @capacity - maximum bytes of contents the file can hold
 *
 * @return vfs_node_t* ref. to the vfs node object of the file created.
 */
vfs_node_t *ramfs_create_file(const char *name, uint8_t *data, uint32_t size,
                              uint32_t capacity);

/**
 * ramfs_populate_initial_tree - create a FS hierarchy
 * root (/)
 * |-- hello.txt
 * |-- etc
 *     |--banner
 *
 * @return vfs error code
 */
int ramfs_seed_root(void);

extern const vfs_node_ops_t ramfs_file_ops;

#endif /* RAMFS_H */
