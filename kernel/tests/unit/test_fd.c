#include "fs/fd.h"
#include "fs/vfs_utils.h"
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
    file = vfs_file_alloc();
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
    int res = vfs_file_free(file);
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
    file = vfs_file_alloc();
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
    file = vfs_file_alloc();
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

uint8_t fd_reuse_test() {
    pcb_t *proc;
    uint8_t fd1, fd2;
    vfs_file_t *file1, *file2;

    /* simulate a process structure */
    proc = pcb_alloc();
    if (!proc) {
        KLOG_ERROR(
            "FD_TEST",
            "reuse test failed. failed to allocate memory to process.\n");
        return 0;
    }
    memset(proc, 0, sizeof(pcb_t));
    proc->pid = 102;
    strncpy(proc->name, "reusetestproc", PROC_NAME_MAX);
    proc->name[PROC_NAME_MAX - 1] = '\0';

    /* create a file 1 and allocate fd */
    file1 = vfs_file_alloc();
    if (!file1) {
        KLOG_ERROR("FD_TEST",
                   "reuse test failed. failed to allocate memory to file 1.\n");
        return 0;
    }
    fd1 = fd_alloc(proc, file1);
    if (fd1 < 0 || fd1 >= PROCESS_MAX_FDS) {
        KLOG_ERROR("FD_TEST",
                   "reuse test failed. fd %u, expected between 0-%u.\n", fd1,
                   PROCESS_MAX_FDS);
        return 0;
    }

    if (proc->fds[fd1] != file1) {
        KLOG_ERROR("FD_TEST",
                   "reuse test failed. fd %u entry in process doesnt point to "
                   "file 1.\n",
                   fd1);
        return 0;
    }

    /* close file 1 */
    int ret1 = fd_close(proc, fd1);
    if (ret1 != VFS_OK) {
        KLOG_ERROR("FD_TEST",
                   "reuse test failed. failed to close file 1. status = %d, "
                   "expected = %d.\n",
                   ret1, VFS_OK);
        return 0;
    }

    /* create a file 2 and allocate fd */
    file2 = vfs_file_alloc();
    if (!file2) {
        KLOG_ERROR("FD_TEST",
                   "reuse test failed. failed to allocate memory to file 2.\n");
        return 0;
    }
    fd2 = fd_alloc(proc, file2);
    if (fd2 != fd1) {
        KLOG_ERROR("FD_TEST",
                   "reuse test failed. fd %u allocated to file 2, expected "
                   "fd %u, same as of file 1 since it was freed.\n",
                   fd2, fd1);
        return 0;
    }

    if (proc->fds[fd2] != file2) {
        KLOG_ERROR("FD_TEST",
                   "reuse test failed. fd %u entry in process doesnt point to "
                   "file 2.\n",
                   fd2);
        return 0;
    }

    /* close file 2 */
    int ret2 = fd_close(proc, fd2);
    if (ret2 != VFS_OK) {
        KLOG_ERROR("FD_TEST",
                   "reuse test failed. failed to close file 2. status = %d, "
                   "expected = %d.\n",
                   ret2, VFS_OK);
        return 0;
    }

    KLOG_INFO("FD_TEST", "reuse test passed.\n");
    return 1;
}

uint8_t fd_close_all_test() {
    pcb_t *proc;
    vfs_file_t *file1, *file2;
    uint32_t fd1, fd2;

    /* allocate memory to simulate a test process */
    proc = pcb_alloc();
    if (!proc) {
        KLOG_ERROR(
            "FD_TEST",
            "close_all test failed. failed to allocate memory to process.\n");
        return 0;
    }
    memset(proc, 0, sizeof(proc));
    strncpy(proc->name, "close_all_test", PROC_NAME_MAX);
    proc->pid = 102;

    /* allocate memory for the file objects */
    file1 = vfs_file_alloc();
    file2 = vfs_file_alloc();
    if (!file1 || !file2) {
        KLOG_ERROR(
            "FD_TEST",
            "close_all test failed. failed to allocate memory for files.\n");
        return 0;
    }

    /* assign file descriptor for the files in the process */
    fd1 = fd_alloc(proc, file1);
    fd2 = fd_alloc(proc, file2);
    if (fd1 != PROCESS_FIRST_NORMAL_FD || fd2 != PROCESS_FIRST_NORMAL_FD + 1) {
        KLOG_ERROR("FD_TEST",
                   "close_all test failed. failed to allocate correct fd for "
                   "process fd1 %u, fd2 %u.\n",
                   fd1, fd2);
        return 0;
    }

    /* close all reference to the files in the process */
    if (fd_close_all(proc) != VFS_OK) {
        KLOG_ERROR("FD_TEST",
                   "close_all test failed. failed to close all files inside "
                   "process %s.\n",
                   proc->name);
        return 0;
    }

    if (proc->fds[fd1] || proc->fds[fd2]) {
        KLOG_ERROR(
            "FD_TEST",
            "close_all test failed. process %s still holds file reference.\n",
            proc->name);
        return 0;
    }

    KLOG_INFO("FD_TEST", "close_all test passed.\n");
    return 1;
}

uint8_t fd_open_path_test() {
    pcb_t *proc;
    vfs_file_t *file;
    int fd;

    /* allocate memory for the process */
    proc = pcb_alloc();
    if (!proc) {
        KLOG_ERROR(
            "FD_TEST",
            "open_path test failed. failed to allocate memory to process.\n");
        return 0;
    }
    memset(proc, 0, sizeof(pcb_t));
    strncpy(proc->name, "open_path_test", PROC_NAME_MAX);
    proc->name[PROC_NAME_MAX - 1] = '\0';

    /* open a file for a process */
    fd = fd_open_path(proc, "/hello.txt", 0);
    if (fd != PROCESS_FIRST_NORMAL_FD) {
        KLOG_ERROR("FD_TEST",
                   "open_path test failed. failed to open path for file "
                   "/hello.txt for process %s. fd = %d, expected fd = %d\n",
                   proc->name, fd, PROCESS_FIRST_NORMAL_FD);
        return 0;
    }

    /* verification */

    /* check if the file referenced by the fd is valid */
    file = fd_get(proc, fd);
    if (!file) {
        KLOG_ERROR("FD_TEST",
                   "open_path test failed. invalid file ref for fd %d.\n", fd);
        return 0;
    }

    /* file should have valid vfs node reference */
    if (!file->node) {
        KLOG_ERROR(
            "FD_TEST",
            "open_path test failed. file has no valid node reference.\n");
        return 0;
    }

    /* compare the file name returned by fd with the one opened */
    if (strcmp(file->node->name, "hello.txt") != 0) {
        KLOG_ERROR("FD_TEST",
                   "open_path test failed. file returned by fd %s, expected "
                   "hello.txt.\n",
                   file->node->name);
        return 0;
    }

    /* validate other file fields */
    if (file->offset != 0) {
        KLOG_ERROR(
            "FD_TEST",
            "open_path test failed. offset for net file %u, expected 0.\n",
            file->offset);
        return 0;
    }

    /* negative case, try opening a file which doesn't exists */
    if (fd_open_path(proc, "/does-not-exist", 0) != VFS_ERR_NOTFOUND) {
        KLOG_ERROR("FD_TEST", "open_path test failed. missing path "
                              "/does-not-exist should have returned null.\n");
        return 0;
    }

    KLOG_INFO("FD_TEST", "open_path test passed.\n");
    return 1;
}

uint8_t fd_read_test() {
    pcb_t *proc;
    int fd;
    int ret;
    char buf[8];

    /* create a process object */
    proc = pcb_alloc();
    if (!proc) {
        KLOG_ERROR(
            "FD_TEST",
            "read test failed. failed to allocate memory for process.\n");
        return 0;
    }
    memset(proc, 0, sizeof(pcb_t));
    strncpy(proc->name, "fd_read_test", PROC_NAME_MAX);
    proc->name[PROC_NAME_MAX - 1] = '\0';
    proc->pid = 102;

    /* initialize the buffer with zeroed memory */
    memset(buf, 0, sizeof(buf));

    /* open a file and associate it with process */
    fd = fd_open_path(proc, "/hello.txt", 0);
    if (fd != PROCESS_FIRST_NORMAL_FD) {
        KLOG_ERROR("FD_TEST",
                   "read test failed. opening file: /hello.txt in proc %s, "
                   "returned fd %u, expected %u.\n",
                   proc->name, fd, PROCESS_FIRST_NORMAL_FD);
        return 0;
    }

    /* read the content of the file */
    ret = fd_read(proc, fd, buf, 5);

    /* verification */
    /* verify the read length */
    if (ret != 5) {
        KLOG_ERROR("FD_TEST",
                   "read test failed. read length %d (%s), expected 5.\n", ret,
                   vfs_get_status_string(ret));
        return 0;
    }

    /* verify the read content */
    if (buf[0] != 'h' && buf[4] != 'o') {
        KLOG_ERROR("FD_TEST",
                   "read test failed. read content = %s, expected = hello.\n",
                   buf);
        return 0;
    }

    /* verify the file offset after read */
    uint32_t offset = fd_get(proc, fd)->offset;
    if (offset != 5) {
        KLOG_ERROR(
            "FD_TEST",
            "read test failed. file offset after read = %u, expected = 5.\n",
            offset);
        return 0;
    }

    KLOG_INFO("FD_TEST", "read test passed.\n");
    return 1;
}
