#include "fs/fd.h"
#include "lib/printk.h"
#include "lib/string.h"
#include "mm/pool_alloc.h"

static pool_allocator_t vfs_file_pool;

void fs_system_init() {
    if (pool_init(&vfs_file_pool, sizeof(vfs_file_t)) < 0) {
        KLOG_ERROR("FD", "failed to intialize vfs_file pool.\n");
        return;
    }

    KLOG_INFO("FD", "vfs_file_pool: memory pool for vfs_file_t successfully "
                    "initialized.\n");
}

vfs_file_t *fs_file_alloc() {
    vfs_file_t *file;

    file = (vfs_file_t *)pool_alloc(&vfs_file_pool);
    if (!file) {
        KLOG_ERROR("FD", "Failed to allocate memory for file object.\n");
        return NULL;
    }

    /* zero out all the bytes of the allocated memory */
    memset(file, 0, sizeof(vfs_file_t));

    file->refcount = 1;

    return file;
}

int fs_file_free(vfs_file_t *file) {
    if (!file) {
        KLOG_ERROR("FD", "file ref to free is NULL.\n");
        return VFS_ERR_INVALID;
    }

    memset(file, 0, sizeof(vfs_file_t));
    pool_free(&vfs_file_pool, file);

    return VFS_OK;
}

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

        fs_file_free(file);
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
