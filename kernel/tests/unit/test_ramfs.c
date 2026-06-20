#include "tests/test_ramfs.h"
#include "fs/ramfs.h"
#include "fs/vfs.h"
#include "fs/vfs_utils.h"

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
    file.data = data;
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

uint8_t ramfs_write_test() {
    static uint8_t storage[16] = {0};
    static uint8_t init[] = "hello";
    static uint8_t overwrite[] = "XY";
    static uint8_t append[] = "!!";

    vfs_node_t *node;
    ramfs_file_t file;

    int write_len;

    /* initialize the vfs tree */
    if (vfs_init() != VFS_OK) {
        KLOG_ERROR("TEST_RAMFS", "write_test failed: vfs init failed.\n");
        return 0;
    }

    /* copy init into storage as initial data */
    storage[0] = init[0];
    storage[1] = init[1];
    storage[2] = init[2];
    storage[3] = init[3];
    storage[4] = init[4];

    /* create the fs backed file object */
    file.data = storage;
    file.size = 5;
    file.capacity = sizeof(storage);

    /* create a vfs node object */
    node = vfs_create_node("write_test.txt", VFS_NODE_FILE, &ramfs_file_ops,
                           &file);
    if (!node) {
        KLOG_ERROR("TEST_RAMFS",
                   "write_test failed: failed creating vfs node object.\n");
        return 0;
    }

    node->size = file.size;

    /* overwrite 2 characters at offset 1 in storage */
    write_len = ramfs_write(node, 1, overwrite, 2);
    if (write_len != 2) {
        KLOG_ERROR(
            "TEST_RAMFS",
            "write_test failed: write length = %u, expected lenth = 2.\n",
            write_len);
        return 0;
    }

    /* check storage for overwriten characters */
    if (storage[0] != 'h' || storage[1] != 'X' || storage[2] != 'Y' ||
        storage[4] != 'o') {
        KLOG_ERROR(
            "TEST_RAMFS",
            "write_test failed: after write storage %s, expected hXYlo.\n",
            storage);
        return 0;
    }

    /* size should have remain unchanges since we overwrite */
    if (file.size != 5) {
        KLOG_ERROR("TEST_RAMFS",
                   "write_test failed: file size changed to %u, expected 5.\n",
                   file.size);
        return 0;
    }

    /* append charactes to the end of the file */
    write_len = ramfs_write(node, 5, append, 2);
    if (write_len != 2) {
        KLOG_ERROR(
            "TEST_RAMFS",
            "write_test failed: write length = %u, expected lenth = 2.\n",
            write_len);
        return 0;
    }

    /* file size should have been updated */
    if (file.size != 7 || node->size != 7) {
        KLOG_ERROR("TEST_RAMFS",
                   "write_test failed: file size is %u, expected 7.\n",
                   file.size);
        return 0;
    }

    /* check updated file content */
    if (storage[0] != 'h' || storage[4] != 'o' || storage[5] != '!' ||
        storage[6] != '!') {
        KLOG_ERROR(
            "TEST_RAMFS",
            "write_test failed: after write storage %s, expected hXYlo!!.\n",
            storage);
        return 0;
    }

    /* test error case - write at wronf offset */
    write_len = ramfs_write(node, 10, append, 2);
    if (write_len != VFS_ERR_INVALID) {
        KLOG_ERROR("TEST_RAMFS",
                   "write_test failed: sparse write should have failed with "
                   "status %u.\n",
                   VFS_ERR_INVALID);
        return 0;
    }

    /* teast error case - write exceeding file capacity */
    write_len = ramfs_write(node, 7, append, 32);
    if (write_len != VFS_ERR_NOMEM) {
        KLOG_ERROR(
            "TEST_RAMFS",
            "write_test failed: capacity overflow should have failed with "
            "status %u.\n",
            VFS_ERR_NOMEM);
    }

    KLOG_INFO("RAMFS", "write_test passed.\n");

    return 1;
}

uint8_t ramfs_create_file_test() {
    static uint8_t data[16] = "hello";
    vfs_node_t *node;
    ramfs_file_t *test_file;

    if (vfs_init() != VFS_OK) {
        KLOG_ERROR("RAMFS_TEST",
                   "create_test failed: failed to initialize vfs.\n");
        return 0;
    }

    node = ramfs_create_file("hello.txt", data, 5, sizeof(data));
    if (!node) {
        KLOG_ERROR("RAMFS_TEST",
                   "create_test failed: failed to create file.\n");
        return 0;
    }

    if (node->type != VFS_NODE_FILE) {
        KLOG_ERROR("RAMFS_TEST",
                   "create_test failed: node type = %s, expected = file.\n",
                   vfs_get_node_type(node->type));
        return 0;
    }

    if (node->ops != &ramfs_file_ops) {
        KLOG_ERROR("RAMFS_TEST", "create_test failed: node ops is invalid.\n");
        return 0;
    }

    if (node->size != 5) {
        KLOG_ERROR("RAMFS_TEST", "create_test failed: node size is invalid.\n");
        return 0;
    }

    test_file = (ramfs_file_t *)node->private_data;
    if (!test_file) {
        KLOG_ERROR("RAMFS_TEST",
                   "create_test failed: node %s, backed data is missing.\n",
                   node->name);
        return 0;
    }

    if (test_file->data != data || test_file->size != 5 ||
        test_file->capacity != sizeof(data)) {
        KLOG_ERROR("RAMFS_TEST",
                   "create_test failed: node %s, backed data is invalid.\n"
                   "\tfile data = %s, expected = %s.\n"
                   "\tfile size = %u, expected = 5.\n"
                   "\tfile capacity = %u, expected = %u.\n",
                   node->name, test_file->data, data, test_file->size,
                   test_file->capacity, sizeof(data));
        return 0;
    }

    if (ramfs_create_file("bad_file.txt", data, 20, 16) != NULL) {
        KLOG_ERROR("RAMFS_TEST", "create_test failed: create file should have "
                                 "failed as size(20) > capacity(16).\n");
        return 0;
    }

    KLOG_INFO("RAMFS_TEST", "create_test succeeded.\n");
    return 1;
}

uint8_t ramfs_initial_vfs_tree_test() {
    vfs_node_t *root;
    vfs_node_t *hello;
    vfs_node_t *etc;
    vfs_node_t *banner;
    char buf[64];
    uint32_t read_len;

    /* initialize the vfs tree
     * create initial tree expects the vfs tree root to be present */
    if (vfs_init() != VFS_OK) {
        KLOG_ERROR(
            "RAMFS_TEST",
            "initail_vfs_tree_test failed. Failed to initialize vfs tree.\n");
        return 0;
    }

    /* check if the root is properly created during init */
    root = vfs_get_root();
    if (!root) {
        KLOG_ERROR(
            "RAMFS_TEST",
            "initial_vfs_tree_test failed. invalid root node for the tree.\n");
        return 0;
    }

    /* create the initial vfs tree */
    if (ramfs_seed_root() != VFS_OK) {
        KLOG_ERROR(
            "RAMFS_TEST",
            "initial_vfs_tree_test failed. failed to populate initial tree.\n");
        return 0;
    }

    /* check 1: lookup file using absolute path /hello.txt */
    hello = vfs_lookup_absolute("/hello.txt");
    if (!hello) {
        KLOG_ERROR("RAMFS_TEST",
                   "intial_vfs_tree_test failed. failed to lookup file "
                   "/hello.txt using absolute path.\n");
        return 0;
    }

    /* check 2: lookup dir using absolute path /etc */
    etc = vfs_lookup_absolute("/etc");
    if (!etc) {
        KLOG_ERROR("RAMFS_TEST", "intial_vfs_tree_test failed. failed to "
                                 "lookup dir /etc using absolute path.\n");
        return 0;
    }

    /* check 3: lookup file nested in subdir using absolute path /etc/banner */
    banner = vfs_lookup_absolute("/etc/banner");
    if (!banner) {
        KLOG_ERROR("RAMFS_TEST", "intial_vfs_tree_test failed. failed to "
                                 "lookup file banner under nested dir /etc.\n");
        return 0;
    }

    /* check 4: read the data from hello.txt */
    read_len = ramfs_read(hello, 0, buf, sizeof(buf));
    if (read_len <= 0 || read_len > sizeof(buf)) {
        KLOG_ERROR("RAMFS_TEST",
                   "intial_vfs_tree_test failed. %s read length = %u, expected "
                   "= %u.\n",
                   hello->name, read_len, sizeof(buf));
        return 0;
    }

    /* check 5: read the data from banner file */
    read_len = ramfs_read(banner, 0, buf, sizeof(buf));
    if (read_len <= 0 || read_len > sizeof(buf)) {
        KLOG_ERROR("RAMFS_TEST",
                   "intial_vfs_tree_test failed. %s read length = %u, expected "
                   "= %u.\n",
                   banner->name, read_len, sizeof(buf));
        return 0;
    }

    KLOG_ERROR("RAMFS_TEST", "initial_vfs_tree test passed.\n");
    return 1;
}
