#include "fs/fd.h"
#include "fs/vfs_utils.h"
#include "lib/printk.h"
#include "lib/string.h"

int fd_alloc(pcb_t *proc, vfs_file_t *file) {
    uint32_t fd;

    if (!proc) {
        KLOG_ERROR("FD", "fd alloc in process failed. process ref. is NULL.\n");
        return VFS_ERR_INVALID;
    }

    if (!file) {
        KLOG_ERROR("FD", "fd alloc in process failed. file ref. is NULL.\n");
        return VFS_ERR_INVALID;
    }

    for (fd = PROCESS_FIRST_NORMAL_FD; fd < PROCESS_MAX_FDS; fd++) {
        if (proc->fds[fd] == NULL) {
            proc->fds[fd] = file;

            return (int)fd;
        }
    }

    return VFS_ERR_NOMEM;
}

vfs_file_t *fd_get(pcb_t *proc, int fd) {

    if (!proc) {
        KLOG_ERROR("FD", "fd get from process failed. process ref. is NULL.\n");
        return NULL;
    }

    if (fd < 0 || fd >= PROCESS_MAX_FDS) {
        KLOG_ERROR("FD", "fd get from process failed. fd (%u) is invalid.\n",
                   fd);
        return NULL;
    }

    if (!proc->fds[fd]) {
        KLOG_ERROR("FD", "fd get from process failed. fd is not present.\n");
        return NULL;
    }

    return proc->fds[fd];
}

int fd_close(pcb_t *proc, int fd) {
    vfs_file_t *file;

    /* check if the process argument is valid */
    if (!proc) {
        KLOG_ERROR("FD", "closing fd failed. process ref is NULL.\n");
        return VFS_ERR_INVALID;
    }

    /* check if the fd argument is valid */
    if (fd < 0 || fd >= PROCESS_MAX_FDS) {
        KLOG_ERROR("FD",
                   "closing fd failed. invalid fd %u, expected between 0-%u.\n",
                   fd, PROCESS_MAX_FDS);
        return VFS_ERR_INVALID;
    }

    /* validate if the process has a valid file entry */
    file = proc->fds[fd];
    if (!file) {
        KLOG_ERROR(
            "FD",
            "closing fd failed. process %u doesnt contain entry for fd %u.\n",
            proc->pid, fd);
        return VFS_ERR_INVALID;
    }

    /* remove the fd entry from the process */
    proc->fds[fd] = NULL;

    /* check for the open references of the file */

    /* case 1: if refcount aready equal 0. return */
    if (file->refcount == 0) {
        KLOG_ERROR("FD", "closing fd failed. file ref. count is already 0.\n");
        return VFS_ERR_INVALID;
    }

    /* case 2: if refcount greater than 1 then just decrease the refcount */
    file->refcount--;

    /* case 3: if refcount after decrease becomes 0. free up resources */
    if (file->refcount == 0) {
        /* check if the associated node is valid and decrease node ref count */
        if (file->node && file->node->refcount > 0) {
            file->node->refcount--;
        }

        vfs_file_free(file);
    }
    KLOG_INFO("FD", "closed fd %u successfully.\n", fd);

    return VFS_OK;
}

int fd_close_all(pcb_t *proc) {
    int ret;
    uint32_t failed_count = 0;

    if (!proc) {
        KLOG_ERROR("FD",
                   "failed to close all fd for process. process is NULL.\n");
        return VFS_ERR_INVALID;
    }

    for (uint32_t fd = 0; fd < PROCESS_MAX_FDS; fd++) {
        if (proc->fds[fd]) {
            ret = fd_close(proc, (int)fd);
            if (ret != VFS_OK) {
                KLOG_WARN(
                    "FD",
                    "proc: %s (%u) close all fds: failed to close fd %u.\n",
                    proc->name, proc->pid, fd);
                failed_count++;
                continue;
            }
        }
    }

    KLOG_INFO("FD",
              "proc %s (%u) close all fds completed. failed closing %u fds.\n",
              proc->name, proc->pid, failed_count);
    return VFS_OK;
}

int fd_install(pcb_t *proc, uint32_t target_fd, vfs_file_t *file) {

    /* check for valid reference to process and path and target fd */
    if (!proc) {
        KLOG_ERROR("FD",
                   "fd_install at fd %u failed. invalid process reference.\n",
                   target_fd);
        return VFS_ERR_INVALID;
    }

    if (!file) {
        KLOG_ERROR("FD",
                   "fd_install for proc %s (pid %u) at fd %u failed. invalid "
                   "file reference.\n",
                   proc->name, proc->pid, target_fd);
        return VFS_ERR_INVALID;
    }

    if (target_fd < 0 || target_fd >= PROCESS_MAX_FDS) {
        KLOG_ERROR(
            "FD",
            "fd_install for process %s (pid %u) at fd %u failed. invalid fd.\n",
            proc->name, proc->pid, target_fd);
        return VFS_ERR_INVALID;
    }

    /* verify if the process has a free slot at target_fd */
    if (proc->fds[target_fd]) {
        KLOG_ERROR("FD",
                   "fd_install for process %s (pid %u) at fd %u failed. fd "
                   "already occupied.\n",
                   target_fd);
        return VFS_ERR_INVALID;
    }

    /* update the fds table to link the file at target_fd */
    proc->fds[target_fd] = file;
    return VFS_OK;
}

int fd_open_path(pcb_t *proc, const char *path, uint32_t flags) {
    vfs_node_t *node;
    vfs_file_t *file;
    int32_t fd;
    int ret;

    /* validate the input arguments */
    if (!path) {
        KLOG_ERROR("FD", "failed to open file. file path is NULL.\n");
        return VFS_ERR_INVALID;
    }

    if (!proc) {
        KLOG_ERROR("FD",
                   "failed to open file %s. process to open for is NULL.\n",
                   path);
        return VFS_ERR_INVALID;
    }

    /* lookup for the vfs node object from the path */
    node = vfs_lookup_absolute(path);
    if (!node) {
        KLOG_ERROR("FD", "open file %s failed. failed to lookup for file.\n",
                   path);
        return VFS_ERR_NOTFOUND;
    }

    if (node->refcount == UINT32_MAX) {
        KLOG_ERROR("FD", "open file %s failed. vfs node ref count %u is max.\n",
                   path, node->refcount);
        return VFS_ERR_NOMEM;
    }

    /* create a vfs file object for the file to be ref. by the process */
    file = vfs_file_alloc();
    if (!file) {
        KLOG_ERROR("FD",
                   "open file %s failed. failed to allocate memory for file.\n",
                   path);
        return VFS_ERR_NOMEM;
    }
    file->node = node;
    file->flags = flags;
    file->offset = 0;
    file->refcount = 1;

    /* update the vfs file object and vfs node object ref. count
     * we have already check for refcount to be inbound */
    node->refcount++;

    /* invoke the open operation for that vfs node */
    if (node->ops && node->ops->open) {
        int ret = node->ops->open(file);
        if (ret != VFS_OK) {
            node->refcount--;
            vfs_file_free(file);
            return ret;
        }
    }

    /* attach the file ref. to the process and return file descriptor */
    fd = fd_alloc(proc, file);
    if (fd < 0) {
        KLOG_ERROR("FD",
                   "open file %s failed. no free fd slot in process %s.\n",
                   path, proc->name);
        node->refcount--;
        vfs_file_free(file);
        return fd;
    }

    KLOG_INFO("FD", "opened path=%s fd=%d flags=0x%x for process %s.\n", path,
              fd, flags, proc->name);
    return fd;
}

int fd_open_path_at(pcb_t *proc, char *path, uint32_t flags, uint32_t fd) {

    vfs_node_t *node;
    vfs_file_t *file;

    int ret;

    /* check for valid reference to process and path */
    if (!proc) {
        KLOG_ERROR("FD",
                   "fd_open_path_at failed. invalid process reference.\n");
        return VFS_ERR_INVALID;
    }

    if (!path) {
        KLOG_ERROR("FD",
                   "fd_open_path_at for process %s (pid %u) failed. invalid "
                   "path string reference.\n",
                   proc->name, proc->pid);
        return VFS_ERR_INVALID;
    }

    /* fd field validation */
    if (fd < 0 || fd >= PROCESS_MAX_FDS) {
        KLOG_ERROR("FD",
                   "fd_open_path_at for process %s (pid %u) failed. invalid fd "
                   "%u to open file at.\n",
                   proc->name, proc->pid, fd);
        return VFS_ERR_INVALID;
    }

    /* vfs node device to be present in the path */
    node = vfs_lookup_absolute(path);
    if (!node) {
        KLOG_ERROR("FD",
                   "fd_open_path_at for process %s (pid %u) failed. error "
                   "finding node for path %s.\n",
                   proc->name, proc->pid, path);
        return VFS_ERR_NOTFOUND;
    }

    /* allocate a file object and reference to node */
    file = vfs_file_alloc();
    if (!file) {
        KLOG_ERROR("FD",
                   "fd_open_path_at for process %s (pid %u) failed. error "
                   "opening file for path %s.\n",
                   proc->name, proc->pid, path);
        return VFS_ERR_NOMEM;
    }
    file->node = node;
    file->flags = flags;
    file->offset = 0;
    file->refcount = 1;

    /* update the device node reference count */
    node->refcount++;

    /* open the file */
    if (!node->ops && !node->ops->open) {
        ret = node->ops->open(file);
        if (ret != VFS_OK) {
            KLOG_ERROR("FD",
                       "fd_open_path_at for process %s (pid %u) failed. error "
                       "opening file at path %s.\n",
                       proc->name, proc->pid, path);
        }
    }

    /* next we link the reference to this file to the process fd table */
    ret = fd_install(proc, fd, file);
    if (ret != VFS_OK) {
        KLOG_ERROR("FD",
                   "fd_open_path_at for process %s (pid %u) failed. failed to "
                   "install file at path %s to fd %u of process.\n",
                   proc->name, proc->pid, path, fd);

        /* do clean up of the file and decrease references */
        node->refcount--;
        vfs_file_free(file);
        return ret;
    }

    KLOG_INFO("FD",
              "successfully opened and linked file at %s to process %s (pid "
              "%u) at fd %u.\n",
              path, proc->name, proc->pid, fd);
    return VFS_OK;
}

int fd_read(pcb_t *proc, int fd, void *buf, uint32_t len) {
    vfs_node_t *node;
    vfs_file_t *file;
    int ret;

    /* validate the input arguments */
    if (!proc) {
        KLOG_ERROR("FD", "fd read failed. invalid process reference.\n");
        return VFS_ERR_INVALID;
    }

    if (!buf && len > 0) {
        KLOG_ERROR("FD", "fd read failed. invalid buf ref. to read into.\n");
        return VFS_ERR_INVALID;
    }

    /* get the vfs file ref. from the process and fd */
    file = fd_get(proc, fd);
    if (!file) {
        KLOG_ERROR("FD",
                   "fd read failed. failed to get file referenced by fd %d in "
                   "process %s.\n",
                   fd, proc->name);
        return VFS_ERR_NOTFOUND;
    }

    /* get the vfs node ref from the file ref. */
    node = file->node;
    if (!node) {
        KLOG_ERROR("FD", "fd read failed. invalid vfs node referenced by the "
                         "vfs file object.\n");
        return VFS_ERR_INVALID;
    }

    if (!node->ops || !node->ops->read) {
        KLOG_ERROR("FD",
                   "fd read failed. file %s node object not associated with "
                   "read operation.\n",
                   node->name);
        return VFS_ERR_NOOP;
    }

    /* invoke the read operation of the vfs node
     * read returns the number of bytes read, on failure returns the negative
     * error code */
    ret = node->ops->read(node, file->offset, buf, len);

    /* update the offset pointer of the file data */
    if (ret > 0) {
        file->offset += (uint32_t)ret;
    }

    KLOG_INFO(
        "FD",
        "fd read completed. fd=%d len=%u bytes=%d new_off=%u status=%s.\n", fd,
        len, ret, file->offset, vfs_get_status_string(ret));
    return ret;
}

int fd_write(pcb_t *proc, int fd, void *buf, uint32_t len) {
    vfs_node_t *node;
    vfs_file_t *file;
    int ret;

    /* validate the input arguments */
    if (!proc) {
        KLOG_ERROR("FD", "fd write failed. invalid process reference.\n");
        return VFS_ERR_INVALID;
    }

    if (!buf && len > 0) {
        KLOG_ERROR("FD", "fd write failed. invalid buf ref. to write from.\n");
        return VFS_ERR_INVALID;
    }

    /* get the vfs file ref. from the process and fd */
    file = fd_get(proc, fd);
    if (!file) {
        KLOG_ERROR("FD",
                   "fd write failed. failed to get file referenced by fd %d in "
                   "process %s.\n",
                   fd, proc->name);
        return VFS_ERR_NOTFOUND;
    }

    /* get the vfs node ref from the file ref. */
    node = file->node;
    if (!node) {
        KLOG_ERROR("FD", "fd write failed. invalid vfs node referenced by the "
                         "vfs file object.\n");
        return VFS_ERR_INVALID;
    }

    if (!node->ops || !node->ops->write) {
        KLOG_ERROR("FD",
                   "fd write failed. file %s node object not associated with "
                   "write operation.\n",
                   node->name);
        return VFS_ERR_NOOP;
    }

    /* invoke the write operation of the vfs node
     * write returns the number of bytes writen, on failure returns the negative
     * error code */
    ret = node->ops->write(node, file->offset, buf, len);

    /* update the offset pointer of the file data */
    if (ret > 0) {
        file->offset += (uint32_t)ret;
    }

    KLOG_VERBOSE(
        "FD",
        "fd write completed. fd=%d len=%u bytes=%d new_off=%u status=%s.\n", fd,
        len, ret, file->offset, vfs_get_status_string(ret));
    return ret;
}

int fd_lseek(pcb_t *proc, int fd, int32_t offset, int whence) {
    vfs_file_t *file;
    vfs_node_t *node;
    int32_t base_offset;
    int32_t new_offset;

    /* validate the input arguments */
    if (!proc) {
        KLOG_ERROR("FD", "fd seek failed. invalid process reference.\n");
        return VFS_ERR_INVALID;
    }

    /* get the file referenced by fd */
    file = fd_get(proc, fd);
    if (!file) {
        KLOG_ERROR("FD",
                   "fd seek failed. failed to get file referenced by fd %d in "
                   "process %s.\n",
                   fd, proc->name);
        return VFS_ERR_NOTFOUND;
    }

    /* get the node reference by the file */
    node = file->node;
    if (!node) {
        KLOG_ERROR("FD", "fd seek failed. invalid vfs node referenced by the "
                         "vfs file object.\n");
        return VFS_ERR_INVALID;
    }

    /* get the base_offset depending upon the seek type
     * offset update is done on the base offset */
    switch (whence) {
    case VFS_SEEK_SET:
        base_offset = 0;
        break;
    case VFS_SEEK_CUR:
        base_offset = file->offset;
        break;
    case VFS_SEEK_END:
        base_offset = (int32_t)node->size;
        break;
    default:
        KLOG_ERROR("FD",
                   "fd seek failed. invalid seek type %d to set base offset "
                   "for update.\n",
                   whence);
        return VFS_ERR_INVALID;
    }

    /* update the base_offset to get the new_offset */
    new_offset = base_offset + offset;

    /* verify the new offset for overflow */
    if (new_offset < 0) {
        KLOG_ERROR("FD",
                   "fd seek failed. offset is invalid after update. base "
                   "offset = %d, offset to update = %d, new offset = %d.\n",
                   base_offset, offset, new_offset);
        return VFS_ERR_INVALID;
    }

    /* update the file offset with the new offset calculated */
    file->offset = (uint32_t)new_offset;
    return file->offset;
}

int fd_setup_stdio(pcb_t *proc) {
    int ret;

    /* check for valid process reference */
    if (!proc) {
        KLOG_ERROR("FD", "setup_stdio failed. invalid process reference.\n");
        return VFS_ERR_INVALID;
    }

    /* open file ref for device /dev/stdin at fd 0 */
    ret = fd_open_path_at(proc, "/dev/stdin", 0, 0);
    if (ret != VFS_OK) {
        KLOG_ERROR("FD",
                   "Failed to setup stdio. linking of /dev/stdin file to fd 0 "
                   "of process %s (pid %u) failed.\n",
                   proc->name, proc->pid);
        return ret;
    }

    /* open file ref for device /dev/stdout at fd 1 */
    ret = fd_open_path_at(proc, "/dev/stdout", 0, 1);
    if (ret != VFS_OK) {
        KLOG_ERROR("FD",
                   "Failed to setup stdio. linking of /dev/stdout file to fd 1 "
                   "of process %s (pid %u) failed.\n",
                   proc->name, proc->pid);
        /* close /dev/stdin on failing to create /dev/stdout */
        fd_close(proc, 0);
        return ret;
    }

    /* open file ref for device /dev/stderr at fd 2 */
    ret = fd_open_path_at(proc, "/dev/stderr", 0, 2);
    if (ret != VFS_OK) {
        KLOG_ERROR("FD",
                   "Failed to setup stdio. linking of /dev/stderr file to fd 2 "
                   "of process %s (pid %u) failed.\n",
                   proc->name, proc->pid);
        /* close /dev/stdin and /dev/stdout on failing to create /dev/stderr */
        fd_close(proc, 0);
        fd_close(proc, 1);
        return ret;
    }

    KLOG_INFO("FD", "setup std io success for process %s (pid %u).\n",
              proc->name, proc->pid);
    return VFS_OK;
}
