#ifndef FD_H
#define FD_H

#include "fs/ramfs.h"
#include "proc/proc.h"

#define S_MAX_OPEN_FILES 64

/**
 * fs_system_init - initialize the memory pool for allocating file object
 * NOTE: This does not initialize the file descriptor tables.
 *
 * @return void
 */
void fs_system_init(void);

/**
 * fs_file_alloc - allocates a memory region for file object from the memory
 * pool and if no free region in the pool then allocate new memory to the pool.
 *
 * @return - ref. to the vfs_file_t object allocated.
 */
vfs_file_t *fs_file_alloc(void);

/**
 * fs_file_free - frees a memory region which was earlier allocated to the file
 * object back to the memory pool
 *
 * @return 1 if successfully free else 0
 */
int fs_file_free(vfs_file_t *file);

/**
 * fd_alloc - allocates  a file descriptor to a file object
 * 0, 1, 2 is reserved for stderr, stdin, stdout
 *
 * @proc - process which is referencing the file
 * @file - file object being referenced
 *
 * @return the file descriptor (index number in fd array)
 */
int fd_alloc(pcb_t *proc, vfs_file_t *file);

/**
 * fd_get - returns the file object based on the given process and the file
 * descriptor.
 *
 * @proc - process referencing the file.
 * @fd - file descriptor to query for the file.
 *
 * @return the file object for fd else null.
 */
vfs_file_t *fd_get(pcb_t *proc, int fd);

/**
 * fd_close - remove the fd entry form the process decreasing the process ref
 * count.
 * if the ref of the file for the process becomes 0 then clean up the file
 * resources and decreate the ref cound of the vfs node backing the file.
 *
 * @proc - process to which the file is associated
 * @fd - of the file to close.
 *
 * @return VFS error codes based upon status
 */
int fd_close(pcb_t *proc, int fd);

#endif /* FD_H */
