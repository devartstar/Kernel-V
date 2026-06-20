#include "tests/test_devfs.h"
#include "fs/devfs.h"
#include "fs/vfs.h"
#include "fs/vfs_utils.h"

static int test_read(vfs_node_t *node, uint32_t offset, void *data,
                     uint32_t len) {
    (void)node;
    (void)offset;
    (void)data;
    return (int)len;
}

static int test_write(vfs_node_t *node, uint32_t offset, const void *data,
                      uint32_t len) {
    (void)node;
    (void)offset;
    (void)data;
    return (int)len;
}

static const vfs_node_ops_t test_chardev_ops = {
    .read = test_read,
    .write = test_write,
    .open = NULL,
};

uint8_t devfs_create_chardev_test() {
    vfs_node_t *char_dev;

    /* initialize the vfs tree */
    if (vfs_init() != VFS_OK) {
        KLOG_ERROR(
            "DEVFS_TEST",
            "create_chardev_test failed. failed to initialize vfs tree.\n");
        return 0;
    }

    /* create a character device */
    char_dev = devfs_create_chardev("test_chardev", &test_chardev_ops, NULL);
    if (!char_dev) {
        KLOG_ERROR(
            "DEVFS_TEST",
            "create_chardev_test failed. fialed to create character device.\n");
        return 0;
    }

    /* device type should be character device */
    if (char_dev->type != VFS_NODE_CHARDEV) {
        KLOG_ERROR("DEVFS_TEST",
                   "create_chardev_test failed. [%s] node type = %s, expected "
                   "= CHARDEV.\n",
                   char_dev->name, vfs_get_node_type(char_dev->type));
    }

    /* chardevice operation should be valid */
    if (!char_dev->ops) {
        KLOG_ERROR("DEVFS_TEST",
                   "create_chardev_test failed. invalid operations of "
                   "character device %s.\n",
                   char_dev->name);
        return 0;
    }

    /* created chardevice should havesize 0 */
    if (char_dev->size != 0) {
        KLOG_ERROR("DEVFS_TEST",
                   "create_chardev_test failed. file must be 0.\n");
        return 0;
    }

    /* test for invalid character device creation */
    if (devfs_create_chardev("bad_dev", NULL, NULL) != NULL) {
        KLOG_ERROR("DEVFS_TEST",
                   "create_chardev_test succeeded. expected failure.\n");
        return 0;
    }

    KLOG_INFO("DEVFS_TEST", "create_chardev_test passed.\n");
    return 1;
}

uint8_t devfs_devnull_test() {
    vfs_node_t *node;
    uint8_t buf[8];
    int read_len, write_len;

    if (vfs_init() != VFS_OK) {
        KLOG_ERROR("DEVFS_TEST",
                   "devnull test failed. failed to initialize vfs tree.\n");
        return 0;
    }

    /* create a null character device */
    node = devfs_create_null();
    if (!node) {
        KLOG_ERROR("DEVFS_TEST",
                   "devnull test failed. failed to create null device.\n");
        return 0;
    }

    /* verify if null device was correctly created */
    if (node->type != VFS_NODE_CHARDEV) {
        KLOG_ERROR(
            "DEVFS_TEST",
            "devnull test failed. device %s type=%s, expected=CHARDEV.\n",
            node->name, vfs_get_node_type(node->type));
        return 0;
    }

    /* verify read to a null device */
    read_len = node->ops->read(node, 0, buf, sizeof(buf));
    if (read_len != 0) {
        KLOG_ERROR("DEVFS_TEST",
                   "devnull test failed. device %s read returned %n bytes, "
                   "expected 0 bytes.\n",
                   node->name, read_len);
        return 0;
    }

    /* verify write to a null device */
    write_len = node->ops->write(node, 0, "abc", 3);
    if (write_len != 3) {
        KLOG_ERROR("DEVFS_TEST",
                   "devnull test failed. device %s write returned %n bytes, "
                   "expected 3 bytes.\n",
                   node->name, write_len);
        return 0;
    }

    /* verify incorrect write to a null device */
    write_len = node->ops->write(node, 0, NULL, 3);
    if (write_len != VFS_ERR_INVALID) {
        KLOG_ERROR("DEVFS_TEST",
                   "devnull test failed. device %s write NULL should have "
                   "failed. returned %u, expected %d\n",
                   node->name, write_len, VFS_ERR_INVALID);
        return 0;
    }

    KLOG_INFO("DEVFS_TEST", "devnull test passed.\n");
    return 1;
}

uint8_t devfs_devzero_test() {
    vfs_node_t *node;
    uint8_t buf[8];
    int read_len, write_len;

    if (vfs_init() != VFS_OK) {
        KLOG_ERROR("DEVFS_TEST",
                   "devzero test failed. failed to initialize vfs tree.\n");
        return 0;
    }

    /* create a zero character device */
    node = devfs_create_zero();
    if (!node) {
        KLOG_ERROR("DEVFS_TEST",
                   "devzero test failed. failed to create zero device.\n");
        return 0;
    }

    /* populate bufer with random data to check if it reads 0 */
    for (uint32_t i = 0; i < (uint32_t)sizeof(buf); i++) {
        buf[i] = 0xAA;
    }

    /* verify if null device was correctly created */
    if (node->type != VFS_NODE_CHARDEV) {
        KLOG_ERROR(
            "DEVFS_TEST",
            "devzero test failed. device %s type=%s, expected=CHARDEV.\n",
            node->name, vfs_get_node_type(node->type));
        return 0;
    }

    /* verify read to a zero device */
    read_len = node->ops->read(node, 0, buf, sizeof(buf));
    if (read_len != (int)sizeof(buf)) {
        KLOG_ERROR("DEVFS_TEST",
                   "devzero test failed. device %s read returned %n bytes, "
                   "expected %u bytes.\n",
                   node->name, read_len, (int)sizeof(buf));
        return 0;
    }

    /* verify all characters read should be 0 */
    for (uint8_t i = 0; i < (int)sizeof(buf); i++) {
        if (buf[i] != 0) {
            KLOG_ERROR("DEVFS_TEST",
                       "devzero test failed. device %s has read a non zero "
                       "value at index %u.\n",
                       node->name, i);
            return 0;
        }
    }

    /* verify write to a null device */
    write_len = node->ops->write(node, 0, "abc", 3);
    if (write_len != 3) {
        KLOG_ERROR("DEVFS_TEST",
                   "devzero test failed. device %s write returned %n bytes, "
                   "expected 3 bytes.\n",
                   node->name, write_len);
        return 0;
    }

    /* verify incorrect write to a null device */
    read_len = node->ops->read(node, 0, NULL, 4);
    if (read_len != VFS_ERR_INVALID) {
        KLOG_ERROR("DEVFS_TEST",
                   "devzero test failed. device %s read NULL should have "
                   "failed. returned %u, expected %d\n",
                   node->name, read_len, VFS_ERR_INVALID);
        return 0;
    }

    KLOG_INFO("DEVFS_TEST", "devzero test passed.\n");
    return 1;
}
