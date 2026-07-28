#include "fs/ramfs.h"
#include "fs/vfs_utils.h"
#include "lib/printk.h"

static ramfs_file_t g_ramfs_files[RAMFS_MAX_FILES];
static uint32_t g_ramfs_file_count = 0;

static void ramfs_memcpy(uint8_t *dest, const uint8_t *src, uint32_t len) {
    uint32_t i;
    for (i = 0; i < len; i++) {
        dest[i] = src[i];
    }
}

int ramfs_read(vfs_node_t *node, uint32_t offset, void *buf, uint32_t len) {
    ramfs_file_t *file;
    uint32_t available;
    uint32_t to_copy;

    /* check validity of the file node to read from */
    if (!node) {
        KLOG_ERROR("RAMFS",
                   "read faile. file vfs node to read from is NULL.\n");
        return VFS_ERR_INVALID;
    }

    /* check validity of the buffer to write into. */
    if (!buf) {
        KLOG_ERROR("RAMFS",
                   "read failed. buf to write the read content is NULL.\n");
        return VFS_ERR_INVALID;
    }

    /* get the file backed data from the vfs node */
    file = (ramfs_file_t *)node->private_data;
    if (!file || !file->data) {
        KLOG_ERROR("RAMFS",
                   "[%s] read failed. invalid ref to backend file data.\n",
                   node->name);
        return VFS_ERR_INVALID;
    }

    /* handle case where length length to read is 0 */
    if (len == 0) {
        KLOG_VERBOSE("RAMFS",
                     "[%s] read completed. read %u bytes from offset %u.\n",
                     node->name, len, offset);
        return 0;
    }

    /* validate if the offset to read is within the file size */
    if (offset >= file->size) {
        KLOG_ERROR(
            "RAMFS",
            "[%s] read failed. offset to read %u exceeds file size %u.\n",
            node->name, offset, file->size);
        return 0;
    }

    /* available bytes to read from offset */
    available = file->size - offset;

    /* handle case bytes to read from offset is more available. Read upto end of
     * the file */
    if (len > available) {
        KLOG_VERBOSE("RAMFS",
                     "[%s] read shrunked. Available bytes to read %u from "
                     "offset %u exceeds length to read %u.\n",
                     node->name, available, offset, len);
        to_copy = available;
    } else {
        to_copy = len;
    }

    /* copy the bytes to the buffer */
    ramfs_memcpy((uint8_t *)buf, file->data + offset, to_copy);

    KLOG_INFO("RAMFS", "[%s] read_completed: bytes=%u off=%u size=%u.\n",
              node->name, to_copy, offset, file->size);

    return (int)to_copy;
}

int ramfs_write(vfs_node_t *node, uint32_t offset, const void *buf,
                uint32_t len) {
    ramfs_file_t *file;
    uint32_t end_offset;

    /* check validity of the node object */
    if (!node) {
        KLOG_ERROR("RAMFS", "write_failed: vfs node object is NULL.\n");
        return VFS_ERR_INVALID;
    }

    /* the write should be on files */
    if (node->type != VFS_NODE_FILE) {
        KLOG_ERROR("RAMFS", "[%s] write_failed: buff is NULL.\n", node->name);
        return VFS_ERR_NOTDIR;
    }

    /* get the file object */
    file = (ramfs_file_t *)node->private_data;

    /* file should be valid and should have data buffer */
    if (!file || !file->data) {
        KLOG_ERROR("RAMFS", "[%s] write_failed: file is invalid.\n",
                   node->name);
        return VFS_ERR_INVALID;
    }

    /* nothing to write */
    if (len == 0) {
        KLOG_VERBOSE("RAMFS", "[%s] write_completed: nothing to write.\n",
                     node->name);
        return 0;
    }

    /* check all possible cases for offset with file size and capacity */

    /* pre-check 1: before start writing offset < filesize */
    if (offset > file->size) {
        KLOG_ERROR(
            "RAMFS",
            "[%s] write_failed: write at offset = %u, over file size = %u.\n",
            node->name, offset, file->size);
        return VFS_ERR_INVALID;
    }

    /* pre-check 2: before start writing offset + len should not overflow */
    if (offset > UINT32_MAX - len) {
        KLOG_ERROR(
            "RAMFS",
            "[%s] write_failed: write offset = %u + length %u, overflows.\n",
            node->name, offset, len);
        return VFS_ERR_INVALID;
    }

    end_offset = offset + len;

    /* case 1: filesize <= capacity <= offset + len */
    if (end_offset > file->capacity) {
        KLOG_ERROR("RAMFS",
                   "[%s] write_failed: write offset %u + len %u exceeds the "
                   "file capacity %u.\n",
                   node->name, offset, len, file->capacity);
        return VFS_ERR_NOMEM;
    }

    /* case 2:  filesize < offset + len <= capacity */
    /* case 3: offset + len <= filesize <= capacity */
    ramfs_memcpy(file->data + offset, buf, len);

    /* update the file size and node size of backing data */
    if (end_offset > file->size) {
        file->size = end_offset;
        node->size = end_offset;
    }

    KLOG_INFO("RAMFS", "[%s] write_completed: bytes=%u off=%u size=%u.\n",
              node->name, len, offset, file->size);

    return (int)len;
}

vfs_node_t *ramfs_create_file(const char *name, uint8_t *data, uint32_t size,
                              uint32_t capacity) {
    ramfs_file_t *file;
    vfs_node_t *node;

    /* name of the file should be valid */
    if (!name) {
        KLOG_ERROR("RAMFS", "failed create_file: name of the file is NULL.\n");
        return NULL;
    }

    /* capacity should not be 0 and data to write less than capacity
     * case where we are writing data to a file of capacity 0 */
    if (!data && capacity != 0) {
        KLOG_ERROR("RAMFS",
                   "[%s] failed create_file: writing data to a file with "
                   "capacity 0.\n",
                   name);
        return NULL;
    }

    if (size > capacity) {
        KLOG_ERROR(
            "RAMFS",
            "[%s] failed create_file: size of data to write %u greater than "
            "capacity %u.\n",
            name, size, capacity);
        return NULL;
    }

    /* file has to be added to the file list - list size < max list size */
    if (g_ramfs_file_count >= RAMFS_MAX_FILES) {
        KLOG_ERROR("RAMFS",
                   "[%s] failed create_file: max file limit %u reached.\n",
                   name, RAMFS_MAX_FILES);
        return NULL;
    }

    /* we are good to create a file object, populate the file object structure
     */
    file = &g_ramfs_files[g_ramfs_file_count++];
    file->data = data;
    file->size = size;
    file->capacity = capacity;

    /* create the vfs node object for the file
     * Multiple vfs_node_t with same name can exixts but not under the same
     * parent. Can 2 different vfs_node_t of same name but different types? */
    node = vfs_create_node(name, VFS_NODE_FILE, &ramfs_file_ops, file);
    if (!node) {
        g_ramfs_file_count--;
        KLOG_ERROR(
            "RAMFS",
            "[%s] failed create_file: failed to create vfs node object.\n",
            name);
        return NULL;
    }

    node->size = size;

    return node;
}

static uint8_t g_hello_storage[64] = "Hello from Kernel-V FS.\n";
static uint8_t g_banner_storage[64] = "Kernel-V RAMFS online.\n";

static int ramfs_seed_file_once(vfs_node_t *parent, const char *name,
                                const char *file_data, uint32_t file_size,
                                uint32_t file_cap) {
    vfs_node_t *node;
    int ret;

    /* check for valid input arguments */
    if (!parent) {
        KLOG_ERROR("RAMFS", "failed seed file. invalid parent reference.\n");
        return VFS_ERR_INVALID;
    }

    if (!name) {
        KLOG_ERROR("RAMFS", "failed seed file. invalid file name reference.\n");
        return VFS_ERR_INVALID;
    }

    if (!file_data) {
        KLOG_ERROR("RAMFS", "failed seed file. invalid reference to data.\n");
        return VFS_ERR_INVALID;
    }

    /* check if the file is already present under the parent then avoid
     * creation, this help us prevent create -> check -> cleanup */
    node = vfs_find_child(parent, name);
    if (!node) {
        /* already exists, seeing already done, just exit */
        return VFS_OK;
    }

    /* create a ramfs file with the given name and data */
    node = ramfs_create_file(name, (uint8_t *)file_data, file_size, file_cap);

    /* add the file under parent */
    ret = vfs_add_child(parent, node);
    if (ret != VFS_OK) {
        // Todo: support cleanup of dile and node
        KLOG_ERROR("RAMFS",
                   "failed seed file. failed to add child %s under parent %s. "
                   "Todo cleanup\n",
                   node->name, parent->name);
        return ret;
    }

    return VFS_OK;
}

static int ramfs_seed_dir_once(vfs_node_t *parent, const char *name) {
    vfs_node_t *node;
    int ret;

    /* check for valid input arguments */
    if (!parent) {
        KLOG_ERROR("RAMFS", "failed seed file. invalid parent reference.\n");
        return VFS_ERR_INVALID;
    }

    node = vfs_find_child(parent, name);
    if (node->type == VFS_NODE_DIR) {
        /* dir already exists, just exit */
        return VFS_OK;
    }

    node = vfs_create_node(name, VFS_NODE_DIR, NULL, NULL);

    /* add the file under parent */
    ret = vfs_add_child(parent, node);
    if (ret != VFS_OK) {
        // Todo: support cleanup of node
        KLOG_ERROR("RAMFS",
                   "failed seed file. failed to add child %s under parent %s. "
                   "Todo cleanup\n",
                   node->name, parent->name);
        return ret;
    }

    return VFS_OK;
}

int ramfs_seed_root() {
    vfs_node_t *root;
    vfs_node_t *etc;
    int ret;

    /* get the VFS root */
    root = vfs_get_root();
    if (!root) {
        KLOG_ERROR(
            "RAMFS",
            "populate initial tree failed. failed to get valid root node.\n");
        return VFS_ERR_INVALID;
    }

    /* NOTE: What if multiple files with the same name is created ?
     * ramfs_create_file -> create a 2 objects:
     * 1. ramfs_file_t instance for the file
     * 2. vfs_node_t instance for the file.
     *       Q. Can a parent vfs node have multiple vfs child node?
     *       A.  of differnt node type - depends.
     *           of same node type - NO.
     *       Fix: use the helper *_seed_once routines.
     */

    /* create a RAMFS file hello.txt */
    ret = ramfs_seed_file_once(root, "hello.txt", g_hello_storage, 23,
                               sizeof(g_hello_storage));
    if (ret != VFS_OK) {
        KLOG_ERROR(
            "RAMFS",
            "populate initial tree failed. failed to create hello.txt file.\n");
        return ret;
    }

    /* create a VFS etc Node Directory */
    ret = ramfs_seed_dir_once(root, "etc");
    if (ret != VFS_OK) {
        KLOG_ERROR("RAMFS",
                   "populate initial tree failed. failed to create etc dir.\n");
        return ret;
    }

    /* create a banner file */
    etc = vfs_find_child(root, "etc");
    ret = ramfs_seed_file_once(etc, "banner", g_banner_storage, 23,
                               sizeof(g_banner_storage));
    if (ret != VFS_OK) {
        KLOG_ERROR(
            "RAMFS",
            "populate initial tree failed. failed to create banner file.\n");
        return ret;
    }

    KLOG_INFO("RAMFS", "initial tree populated.\n");

    return VFS_OK;
}

const vfs_node_ops_t ramfs_file_ops = {
    .open = NULL,
    .read = ramfs_read,
    .write = ramfs_write,
};
