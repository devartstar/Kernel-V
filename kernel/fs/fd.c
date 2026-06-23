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
