#include "fs/devfs.h"
#include "drivers/console_input.h"
#include "drivers/serial.h"
#include "drivers/vga.h"
#include "fs/vfs_utils.h"
#include "lib/printk.h"

#define DEVFS_CONSOLE_COLOR 0x07

typedef vfs_node_t *(*create_device_routine)(void);

typedef struct _char_device_list_entry {
    char *name;
    create_device_routine devfs_create;
} char_device_list_entry;

char_device_list_entry char_device_list[] = {
    {
        .name = "zero",
        .devfs_create = devfs_create_zero,
    },
    {
        .name = "null",
        .devfs_create = devfs_create_null,
    },
    {
        .name = "console",
        .devfs_create = devfs_create_console,
    },
    {
        .name = "stdin",
        .devfs_create = devfs_create_stdin,
    },
    {
        .name = "stdout",
        .devfs_create = devfs_create_stdout,
    },
    {
        .name = "stderr",
        .devfs_create = devfs_create_stderr,
    },
};

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

static void devfs_memzero(uint8_t *buf, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = 0;
    }
}

/* *** /dev/null: NULL DEVICE OPERATIONS *** */

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
    if (!buf && len > 0) {
        KLOG_ERROR(
            "DEVFS",
            "dev/null write failed. invalid buffer or length to write.\n");
        return VFS_ERR_INVALID;
    }

    KLOG_INFO("DEVFS", "dev/null write succeeded.\n");
    return (int)len;
}

/* *** /dev/zero: ZERO DEVICE OPERATIONS *** */

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

    if (len == 0) {
        return 0;
    }

    /* validate for length and valid buffer */
    if (!buf & len > 0) {
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

    /* check if the buffer to write is valid */
    if (!buf && len > 0) {
        KLOG_ERROR(
            "DEVFS",
            "dev/zero write failed. invalid buffer or length to write.\n");
        return VFS_ERR_INVALID;
    }

    KLOG_INFO("DEVFS", "dev/zero write succeeded.\n");
    return (int)len;
}

/* *** INPUT/OUTPUT DEVICE OPERATIONS *** */

/**
 * devconsole_write -  write to the file associated to the /dev/console device
 * object
 * @node - reference to the vfs node backing the device
 * @offset - offset of the file to write the contents
 * @buf -  reference to the buffer containing the contents to write
 * @len - length of the characters to write
 *
 * @return length of the contents written
 */
static int devconsole_write(vfs_node_t *node, uint32_t offset, const void *buf,
                            uint32_t len) {

    (void)node;
    (void)offset;

    if (len == 0) {
        return 0;
    }

    /* check for the validity of the buffer passed */
    if (!buf) {
        KLOG_VERBOSE("DEVFS",
                     "console_write failed. buffer reference is invalid.\n");
        return VFS_ERR_INVALID;
    }

    const char *data = (const char *)buf;

    /*
     * Write the buffer to the console output
     * Do not write to log buffer
     */
    vga_write(data, len, DEVFS_CONSOLE_COLOR);
    serial_write(data, len);

    KLOG_VERBOSE("DEVFS", "Successfully write to console.\n");
    return (int)len;
}

/**
 * devconsole_read - read from a file associated to the /dev/console device
 * object
 * @node - reference to the vfs node backing the device
 * @offset - offset of the file from read from
 * @buf - refernce to the buffer containing the contents to write
 * @len - length of the characters to write
 *
 * @return length of the contents read
 */
static int devstdin_read(vfs_node_t *node, uint32_t offset, void *buf,
                         uint32_t len) {
    (void)offset;

    uint32_t read_len = 0;

    /* check if the backed vfs node is valid */
    if (!node) {
        KLOG_ERROR("DEVFS", "devstdin_read failed. invalid vfs node object.\n");
        return VFS_ERR_INVALID;
    }

    /* check if the device readind should be a character device */
    if (node->type != VFS_NODE_CHARDEV) {
        KLOG_ERROR("DEVFS", "devstdin failed. not a character device.\n");
        return VFS_ERR_INVALID;
    }

    if (len == 0) {
        return 0;
    }

    /* check if the buffer to read into is valid */
    if (!buf) {
        KLOG_ERROR("DEVFS",
                   "devstdin_read failed. invalid buffer to copy to.\n");
        return VFS_ERR_INVALID;
    }

    /**
     * NOTE: Communication between UART(COM port) and Serial Driver can be done
     * in two ways:
     * 1. Continous polling - stdin read keeps polling continously until UART
     * has data avaiable
     * 2. Interrupt Based signal - UART asserts IRQ4 when it has data available
     */

    /* THIS is needed for continous polling ONLY,
     * drain any pending bytes from UART through serial driver to console buffer
     * NOTE: currently IRQ 4 -> mapped -> serial int handler -> stored the data
     * when UART asserts IRQ.
     * serial_dump_input_to_console();
     * /

    /* read the buffer from the console */
    read_len = console_input_read((char *)buf, len);

    /* console buffer exists but nothing to read */
    if (read_len == 0) {
        KLOG_ERROR("DEVFS", "devstdin_read failed. console buffer exists but "
                            "no characters to read.\n");
        return VFS_ERR_AGAIN;
    }

    KLOG_VERBOSE("DEVFS", "Successfully read %u characters from console.\n",
                 read_len);
    return (int)read_len;
}

/** *** REGISTER OPERATIONS FOR DEVICES *** */

const vfs_node_ops_t devnull_ops = {
    .open = NULL,
    .read = devnull_read,
    .write = devnull_write,
};

const vfs_node_ops_t devzero_ops = {
    .open = NULL,
    .read = devzero_read,
    .write = devzero_write,
};

const vfs_node_ops_t devconsole_ops = {
    .open = NULL,
    .read = devstdin_read,
    .write = devconsole_write,
};

const vfs_node_ops_t devstdin_ops = {
    .open = NULL,
    .read = devstdin_read,
    .write = NULL,
};

const vfs_node_ops_t devstdout_ops = {
    .open = NULL,
    .write = devconsole_write,
    .read = NULL,
};

const vfs_node_ops_t devstderr_ops = {
    .open = NULL,
    .write = devconsole_write,
    .read = NULL,
};

/** *** CREATE DEVICES *** */

vfs_node_t *devfs_create_zero() {
    return devfs_create_chardev("zero", &devzero_ops, NULL);
}

vfs_node_t *devfs_create_null(void) {
    return devfs_create_chardev("null", &devnull_ops, NULL);
}

vfs_node_t *devfs_create_console(void) {
    return devfs_create_chardev("console", &devconsole_ops, NULL);
}

vfs_node_t *devfs_create_stdin(void) {
    return devfs_create_chardev("stdin", &devstdin_ops, NULL);
}

vfs_node_t *devfs_create_stdout(void) {
    return devfs_create_chardev("stdout", &devstdout_ops, NULL);
}

vfs_node_t *devfs_create_stderr(void) {
    return devfs_create_chardev("stderr", &devstderr_ops, NULL);
}

/** *** ATTACH THE DEVFS UNDER ROOT *** */
int devfs_seed_root() {
    vfs_node_t *root;
    vfs_node_t *dev;
    vfs_node_t *chardev;

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
                   dev->name, vfs_get_node_type(dev->type),
                   vfs_get_node_type(VFS_NODE_DIR));
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

    /* create a standard input/output devices */
    uint8_t chardev_count =
        sizeof(char_device_list) / sizeof(char_device_list_entry);
    for (uint8_t i = 0; i < chardev_count; i++) {
        chardev = vfs_find_child(dev, char_device_list[i].name);
        if (chardev) {
            /* device already seeded under /dev; keep seeding idempotent */
            continue;
        }

        chardev = char_device_list[i].devfs_create();
        if (!chardev) {
            KLOG_ERROR("DEVFS",
                       "seeding /dev failed. failed to create vfs_node %s.\n",
                       char_device_list[i].name);
            return VFS_ERR_NOMEM;
        }

        ret = vfs_add_child(dev, chardev);
        if (ret != VFS_OK) {
            KLOG_ERROR("DEVFS",
                       "seeding /dev failed. failed to link %s to parent %s.\n",
                       chardev->name, dev->name);
            return ret;
        }

        KLOG_INFO("DEVFS",
                  "successfully added device %s to parent device %s.\n",
                  chardev->name, dev->name)
    }

    KLOG_INFO("DEVFS", "seeding /dev succeeded.\n");
    return VFS_OK;
}
