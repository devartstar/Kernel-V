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
        if (!file->node && file->node->refcount > 0) {
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

int fd_open_path(pcb_t *proc, const char *path, uint32_t flags) {
    vfs_node_t *node;
    vfs_file_t *file;
    uint32_t fd;
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
        KLOG_ERROR("FD", "open file %s failed. failed to lookup for file.\n");
        return VFS_ERR_NOTFOUND;
    }

    if (node->refcount == UINT32_MAX) {
        KLOG_ERROR("FD", "open file %s failed. vfs node ref count %u is max.\n",
                   node->refcount);
        return VFS_ERR_NOMEM;
    }

    /* create a vfs file object for the file to be ref. by the process */
    file = vfs_file_alloc();
    if (!file) {
        KLOG_ERROR(
            "FD", "open file %s failed. failed to allocate memory for file.\n");
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
        node->refcount--;
        vfs_file_free(fd);
        return fd;
    }

    KLOG_INFO("FD", "successfully opened file %s for process %s.\n", path,
              proc->name);
    return fd;
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
    if (ret >= 0) {
        file->offset += (uint32_t)ret;
    }

    KLOG_INFO("FD", "fd read completed. status = %s.\n",
              vfs_get_status_string(ret));
    return ret;
}
