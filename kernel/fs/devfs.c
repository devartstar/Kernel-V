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

/* *** /dev/null: NULL DEVICE START *** */

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

/* *** /dev/zero: ZERO DEVICE START *** */

static void devfs_memzero(uint8_t *buf, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = 0;
    }
}

static int devzero_read(vfs_node_t *node, uint32_t offset, void *buf,
                        uint32_t len) {
    (void)offset;

    /* just verify if node is valid zero device */
    if (!node) {
        KLOG_ERROR("DEVFS", "dev/zero read failed. invalid vfs node object.\n");
        return VFS_ERR_INVALID;
    }

    if (node->type != VFS_NODE_CHARDEV) {
        KLOG_ERROR(
            "DEVFS",
            "dev/zero read failed. node type = %s, expected = CHARDEV.\n",
            vfs_get_node_type(node->type));
        return VFS_ERR_INVALID;
    }

    /* validate for length and valid buffer */
    if (len < 0 || !buf) {
        KLOG_ERROR(
            "DEVFS",
            "/dev/zero read failed. invalid buffer or length to read.\n");
        return VFS_ERR_INVALID;
    }

    devfs_memzero((uint8_t *)buf, len);

    KLOG_INFO("DEVFS", "dev/zero read succeeded.\n");
    return len;
}

static int devzero_write(vfs_node_t *node, uint32_t offset, const void *buf,
                         uint32_t len) {
    (void)offset;

    /* just verify if node is valid character device */
    if (!node) {
        KLOG_ERROR("DEVFS",
                   "dev/zero write failed. invalid vfs node object.\n");
        return VFS_ERR_INVALID;
    }

    if (node->type != VFS_NODE_CHARDEV) {
        KLOG_ERROR(
            "DEVFS",
            "dev/zero write failed. node type = %s, expected = CHARDEV.\n",
            vfs_get_node_type(node->type));
        return VFS_ERR_INVALID;
    }

    /* ceck if the buffer to write is valid */
    if (len < 0 || !buf) {
        KLOG_ERROR(
            "DEVFS",
            "dev/zero write fialed. invalid buffer or length to write.\n");
        return VFS_ERR_INVALID;
    }

    KLOG_INFO("DEVFS", "dev/zero write succeeded.\n");
    return (int)len;
}

const vfs_node_ops_t devzero_ops = {
    .open = NULL,
    .read = devzero_read,
    .write = devzero_write,
};

vfs_node_t *devfs_create_zero() {
    return devfs_create_chardev("zero", &devzero_ops, NULL);
}

/** *** CREATE/ATTACH THE DEVFS UNDER ROOT *** */
int devfs_seed_root() {
    vfs_node_t *root;
    vfs_node_t *dev;
    vfs_node_t *null;
    vfs_node_t *zero;
    int ret;

    /* get the rot of the vfs */
    root = vfs_get_root();
    if (!root) {
        KLOG_ERROR("DEVFS", "seeding /dev failed. Failed to get root node.\n");
        return VFS_ERR_INVALID;
    }

    /* check if dev already exists and valid under root directory */
    dev = vfs_find_child(root, "dev");
    if (dev && dev->type != VFS_NODE_DIR) {
        KLOG_ERROR("DEVFS",
                   "seeding /dev failed. /dev already exists. %s is of type "
                   "%s, expected=%s.\n",
                   dev->name, vfs_get_node_type(dev->type));
        return VFS_ERR_NOTDIR;
    }

    if (!dev) {
        /* create a vfs node dev under root as the base of devfs */
        dev = vfs_create_node("dev", VFS_NODE_DIR, NULL, NULL);
        if (!dev) {
            KLOG_ERROR("DEVFS",
                       "seeding /dev failed. failed to create vfs node dev.\n");
            return VFS_ERR_NOMEM;
        }

        ret = vfs_add_child(root, dev);
        if (ret != VFS_OK) {
            KLOG_ERROR(
                "DEVFS",
                "seeding /dev failed. failed to link child %s to parent %s.\n",
                dev->name, root->name);
            return ret;
        }
    }

    /* create a null character device and attach it to devfs root */
    null = vfs_find_child(dev, "null");
    if (!null) {
        null = devfs_create_null();
        if (!null) {
            KLOG_ERROR(
                "DEVFS",
                "seeding /dev failed. failed to create vfs node null.\n");
            return VFS_ERR_NOMEM;
        }

        ret = vfs_add_child(dev, null);
        if (ret != VFS_OK) {
            KLOG_ERROR(
                "DEVFS",
                "seeding /dev failed. failed to link child %s to parent %s.\n",
                null->name, dev->name);
            return ret;
        }
    }

    /* create a zero character device and attach it to devfs root */
    zero = vfs_find_child(dev, "zero");
    if (!zero) {
        zero = devfs_create_zero();
        if (!zero) {
            KLOG_ERROR(
                "DEVFS",
                "seeding /dev failed. failed to create vfs node zero.\n");
            return VFS_ERR_NOMEM;
        }

        ret = vfs_add_child(dev, zero);
        if (ret != VFS_OK) {
            KLOG_ERROR(
                "DEVFS",
                "seeding /dev failed. failed to link child %s to parent %s.\n",
                zero->name, dev->name);
            return ret;
        }
    }

    KLOG_INFO("DEVFS", "seeding /dev succeeded.\n");
    return VFS_OK;
}
