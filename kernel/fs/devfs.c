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
