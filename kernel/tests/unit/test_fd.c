#include "fs/fd.h"
#include "lib/printk.h"

int fd_alloc_free_test() {
    pcb_t *proc;
    vfs_file_t *file;

    /* create a pcb object */
    proc = pcb_alloc();
    if (!proc) {
        KLOG_ERROR(
            "FD_TEST",
            "alloc_free test failed. failed to allocate memory for pcb.\n");
        return 0;
    }

    /* allocate 0 to the pcb object */
    memset(proc, 0, sizeof(pcb_t));

    /* create a file object */
    file = fs_file_alloc();
    if (!file) {
        KLOG_ERROR(
            "FD_TEST",
            "alloc_free test failed. failed to allocate memory for file.\n");
        return 0;
    }

    /* allocate file desctiptor for the file object */
    int fd = fd_alloc(proc, file);

    /* verificatiom */
    if (fd != PROCESS_FIRST_NORMAL_FD) {
        KLOG_ERROR(
            "FD_TEST",
            "alloc_free test failed. failed to allocate file descriptor. fd "
            "= %u, expected fd = %u.\n",
            fd, PROCESS_FIRST_NORMAL_FD);
        return 0;
    }

    if (proc->fds[fd] != file) {
        KLOG_ERROR("FD_TEST", "alloc_free test failed. for fd = %u expected "
                              "file to be attached.\n");
        return 0;
    }

    /* free the memory allocated for the file back to the pool */
    int res = fs_file_free(file);
    if (res != VFS_OK) {
        KLOG_ERROR("FD_TEST", "alloc_free test failed. failed to free "
                              "allocated memory for file.\n");
        return 0;
    }

    /* free the memory allocated for the pcb */
    pcb_free(proc);

    KLOG_INFO("FD_TEST", "alloc test passed.\n");
    return 1;
}

uint8_t fd_get_test() {
    pcb_t *proc;
    vfs_file_t *file;

    /* allocate memory for pcb block */
    proc = pcb_alloc();
    if (!proc) {
        KLOG_ERROR("FD_TEST",
                   "get test failed. failed to allocate memory for pcb.\n");
        return 0;
    }

    /* zero out the memory allocated to the pcb object */
    memset(proc, 0, sizeof(pcb_t));

    /* allocates zeroed memory for a file object */
    file = fs_file_alloc();
    if (!file) {
        KLOG_ERROR("FD_TEST",
                   "get test failed. failed to allocate memory for file.\n");
        return 0;
    }

    /* allocate file descriptor for the file in the process */
    int fd = fd_alloc(proc, file);
    if (fd != PROCESS_FIRST_NORMAL_FD) {
        KLOG_ERROR("FD_TEST",
                   "get test failed. failed to allocate file descriptor. fd = "
                   "%u, expected fd = %u\n",
                   fd, PROCESS_FIRST_NORMAL_FD);
        return 0;
    }

    /* verification */

    /* case 1: try to get file from correct fd */
    if (fd_get(proc, fd) != file) {
        KLOG_ERROR("FD_TEST",
                   "get test failed. failed to get file from fd %u.\n", fd);
        return 0;
    }

    /* case 2: try to get file from a negative fd */
    if (fd_get(proc, -1) != NULL) {
        KLOG_ERROR("FD_TEST", "get test failed. recieved file from negative fd "
                              "-1. expected NULL\n");
        return 0;
    }

    /* case 3: try to get file from fd above max fd */
    if (fd_get(proc, PROCESS_MAX_FDS) != NULL) {
        KLOG_ERROR("FD_TEST",
                   "get test failed. recieved file from fd %u out of range.\n",
                   PROCESS_MAX_FDS);
        return 0;
    }

    /* case 4: try to get file from un-allocated fd */
    if (fd_get(proc, 10) != NULL) {
        KLOG_ERROR("FD_TEST",
                   "get test failed. recieved file from unallocated fd 10. "
                   "expected NULL\n",
                   PROCESS_MAX_FDS);
        return 0;
    }

    /* case 5: try to get file from invalid process */
    if (fd_get(NULL, fd) != NULL) {
        KLOG_ERROR(
            "FD_TEST",
            "get test failed. recieved file from fd %u from a NULL process.\n",
            PROCESS_MAX_FDS);
        return 0;
    }

    KLOG_INFO("FD_TEST", "get fd test passed.\n");
    return 1;
}

uint8_t fd_close_test() {
    pcb_t *proc;
    vfs_file_t *file;

    /* allocate memory for pcb block */
    proc = pcb_alloc();
    if (!proc) {
        KLOG_ERROR("FD_TEST",
                   "close test failed. failed to allocate memory for pcb.\n");
        return 0;
    }

    /* zero out the memory allocated to the pcb object */
    memset(proc, 0, sizeof(pcb_t));
    proc->pid = 102;
    strncpy(proc->name, "fdclose", PROC_NAME_MAX);
    proc->name[PROC_NAME_MAX - 1] = '\0';

    /* allocates zeroed memory for a file object */
    file = fs_file_alloc();
    if (!file) {
        KLOG_ERROR("FD_TEST",
                   "close test failed. failed to allocate memory for file.\n");
        return 0;
    }

    /* allocate file descriptor for the file in the process */
    int fd = fd_alloc(proc, file);
    if (fd != PROCESS_FIRST_NORMAL_FD) {
        KLOG_ERROR(
            "FD_TEST",
            "close test failed. failed to allocate file descriptor. fd = "
            "%u, expected fd = %u\n",
            fd, PROCESS_FIRST_NORMAL_FD);
        return 0;
    }

    /* verification */

    /* case 1: close the file  */
    if (fd_close(proc, fd) != VFS_OK) {
        KLOG_ERROR("FD_TEST",
                   "close test failed. close fd error, expected %u.\n", VFS_OK);
        return 0;
    }

    /* case 2: failure case to get the closed file */
    if (fd_get(proc, fd) != NULL) {
        KLOG_ERROR(
            "FD_TEST",
            "close test failed. querying closed fd returned non NUll ref.\n");
        return 0;
    }

    /* case 3: failure case, close an already closed file */
    if (fd_close(proc, fd) == VFS_OK) {
        KLOG_ERROR("FD_TEST", "close test failed. closing previously closed "
                              "file returned success.\n");
        return 0;
    }

    KLOG_INFO("FD_TEST", "close test passed.\n");
    return 1;
}
