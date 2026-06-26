#ifndef FD_H
#define FD_H

#include "fs/ramfs.h"
#include "proc/proc.h"

#define S_MAX_OPEN_FILES 64

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

/**
 * fd_close_all - close all the file ref. for all file descriptor of the process
 * @proc - process to close all associated files.
 *
 * @return status of closing all file.
 */
int fd_close_all(pcb_t *proc);

/**
 * fd_open_path - open the file at the given absolute path for a process.
 * looks up the vfs node for the path, allocates a vfs file object referencing
 * it, invokes the node's open operation (if any) and attaches the file to the
 * process by allocating a file descriptor.
 *
 * @proc - process opening the file.
 * @path - absolute path of the file to open.
 * @flags - open flags stored on the resulting file object.
 *
 * @return the allocated file descriptor on success, else a negative VFS error
 * code.
 */
int fd_open_path(pcb_t *proc, const char *path, uint32_t flags);

#endif /* FD_H */
