#include "fs/devfs.h"
#include "fs/vfs_utils.h"
#include "lib/printk.h"

vfs_node_t *devfs_create_chardev(const char *name, const vfs_node_ops_t *ops,
                                 void *private_data) {
    vfs_node_t *char_dev;

    /* verify the name of the character device */
    if (!name) {
        KLOG_ERROR("DEVFS", "create_chardev failed. invalid device name.\n");
        return NULL;
    }

    /* verify the vfs node operation object passed as arg */
    if (!ops) {
        KLOG_ERROR("DEVFS",
                   "create_chardev failed. invalid operation object.\n");
        return NULL;
    }

    /* create a character device vfs node object */
    char_dev = vfs_create_node(name, VFS_NODE_CHARDEV, ops, private_data);
    if (!char_dev) {
        KLOG_ERROR(
            "DEVFS",
            "create_chardev failed. failed to create a character device.\n");
        return NULL;
    }

    char_dev->size = 0;

    KLOG_INFO("DEVFS", "create_chardev succeeded.\n");
    return char_dev;
}

/** *** /dev/null: NULL DEVICE START *** */

static int devnull_read(vfs_node_t *node, uint32_t offset, void *buf,
                        uint32_t len) {
    (void)offset;
    (void)buf;
    (void)len;

    /* just verify if node is valid character device */
    if (!node) {
        KLOG_ERROR("DEVFS", "dev/null read failed. invalid vfs node object.\n");
        return VFS_ERR_INVALID;
    }

    if (node->type != VFS_NODE_CHARDEV) {
        KLOG_ERROR(
            "DEVFS",
            "dev/null read failed. node type = %s, expected = CHARDEV.\n",
            vfs_get_node_type(node->type));
        return VFS_ERR_INVALID;
    }

    KLOG_INFO("DEVFS", "dev/null read succeeded.\n");
    return 0;
}

static int devnull_write(vfs_node_t *node, uint32_t offset, const void *buf,
                         uint32_t len) {

    (void)offset;

    /* just verify if node is valid character device */
    if (!node) {
        KLOG_ERROR("DEVFS",
                   "dev/null write failed. invalid vfs node object.\n");
        return VFS_ERR_INVALID;
    }

    if (node->type != VFS_NODE_CHARDEV) {
        KLOG_ERROR(
            "DEVFS",
            "dev/null write failed. node type = %s, expected = CHARDEV.\n",
            vfs_get_node_type(node->type));
        return VFS_ERR_INVALID;
    }

    /* ceck if the buffer to write is valid */
    if (len < 0 || !buf) {
        KLOG_ERROR(
            "DEVFS",
            "dev/null write fialed. invalid buffer or length to write.\n");
        return VFS_ERR_INVALID;
    }

    KLOG_INFO("DEVFS", "dev/null write succeeded.\n");
    return (int)len;
}

const vfs_node_ops_t devnull_ops = {
    .open = NULL,
    .read = devnull_read,
    .write = devnull_write,
};

vfs_node_t *devfs_create_null(void) {
    return devfs_create_chardev("null", &devnull_ops, NULL);
}
