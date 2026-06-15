#include "tests/test_ramfs.h"
#include "fs/ramfs.h"
#include "fs/vfs.h"

uint8_t ramfs_read_test() {
    static uint8_t data[] = "hello";
    vfs_node_t *node;
    ramfs_file_t file;
    char buf[8];
    uint8_t read_len;

    /* initialize the vfs layer */
    if (vfs_init() != VFS_OK) {
        KLOG_ERROR("TEST_RAMFS",
                   "read_test failed. failed to initialize vfs.\n");
        return 0;
    }

    /* intialize the fs file object */
    file.data = &data;
    file.size = 5;
    file.capacity = 5;

    /* assign a vfs node for file */
    node = vfs_create_node("test.txt", VFS_NODE_FILE, &ramfs_file_ops, &file);
    if (!node) {
        KLOG_ERROR("TEST_RAMFS",
                   "read_test failed. failed to create node object.\n");
        return 0;
    }

    /* read: offset = 0, length = 5, expected = "hello" */
    read_len = ramfs_read(node, 0, buf, 5);
    if (read_len != 5 || buf[0] != 'h' || buf[4] != 'o') {
        KLOG_ERROR("TEST_RAMFS",
                   "read_test failed. bytes (read = %u, expected = 5), data "
                   "(read = %s, expected = hello).\n",
                   read_len, buf);
        return 0;
    }

    /* read: offset = 2, length = 8, expected = "llo" */
    read_len = ramfs_read(node, 2, buf, 8);
    if (read_len != 3 || buf[0] != 'l' || buf[2] != 'o') {
        KLOG_ERROR("TEST_RAMFS",
                   "read_test failed. bytes (read = %u, expected = 2), data "
                   "(read = %s, expected = lo).\n",
                   read_len, buf);
        return 0;
    }

    /* read: offset = 5, length = 8, expected = "" */
    read_len = ramfs_read(node, 5, buf, 8);
    if (read_len != 0) {
        KLOG_ERROR("TEST_RAMFS",
                   "read_test failed. bytes (read = %u, expected = 0), data "
                   "(read = %s, expected nothing).\n",
                   read_len, buf);
        return 0;
    }

    /* read: offset = 99, length = 8, expected = "" */
    read_len = ramfs_read(node, 99, buf, 8);
    if (read_len != 0) {
        KLOG_ERROR("TEST_RAMFS",
                   "read_test failed. bytes (read = %u, expected = 0), data "
                   "(read = %s, expected nothing).\n",
                   read_len, buf);
        return 0;
    }

    KLOG_INFO("TEST_RAMFS", "read_test successful.\n");
    return 1;
}
