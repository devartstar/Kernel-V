#include "fs/ramfs.h"
#include "fs/vfs_utils.h"
#include "lib/printk.h"

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

    KLOG_INFO("RAMFS",
              "[%s] read_completed: read %s from offset %u of the file.\n",
              node->name, buf, offset);

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
            node->name, len);
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

    KLOG_INFO("RAMFS",
              "[%s] write_completed: wrote %s at offset %u to the file.\n",
              node->name, buf, offset);

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

    /* create the vfs node object for the file */
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
int ramfs_populate_intial_tree() {
    vfs_node_t *root;
    vfs_node_t *hello;
    vfs_node_t *etc;
    vfs_node_t *banner;

    /* get the VFS root */
    root = vfs_get_root();
    if (!root) {
        KLOG_ERROR(
            "RAMFS",
            "populate initial tree failed. failed to get valid root node.\n");
        return VFS_ERR_INVALID;
    }

    /* create a RAMFS file hello.txt */
    hello = ramfs_create_file("hello.txt", g_hello_storage, 23,
                              sizeof(g_hello_storage));
    if (!hello) {
        KLOG_ERROR(
            "RAMFS",
            "populate initial tree failed. failed to create hello.txt file.\n");
        return VFS_ERR_NOMEM;
    }

    /* add hello.txt file under the VFS root */
    if (vfs_add_child(root, hello) != VFS_OK) {
        KLOG_ERROR(
            "RAMFS",
            "populate initial tree failed. failed to add child %s (type %s) to "
            "parent %s (type %s). \n",
            hello->name, vfs_get_node_type(hello->type), root->name,
            vfs_get_node_type(root->type));
        return VFS_ERR_INVALID;
    }

    /* create a VFS etc Node Directory */
    etc = vfs_create_node("etc", VFS_NODE_DIR, NULL, NULL);
    if (!etc) {
        KLOG_ERROR(
            "RAMFS",
            "populate intial tree failed. failed to create etc (directory).\n");
        return VFS_ERR_NOMEM;
    }

    /* add etc directory under root directory */
    if (vfs_add_child(root, etc) != VFS_OK) {
        KLOG_ERROR("RAMFS",
                   "populate initial tree failed. failed to add child %s (type "
                   "%s) to parent %s (type %s).\n",
                   etc->name, vfs_get_node_type(etc->type), root->name,
                   vfs_get_node_type(root->type));
    }

    /* create a banner file */
    banner = ramfs_create_file("banner", g_banner_storage, 23,
                               sizeof(g_banner_storage));
    if (!banner) {
        KLOG_ERROR(
            "RAMFS",
            "populate initial tree failed. failed to create banner file.\n");
        return VFS_ERR_NOMEM;
    }

    /* add the banner file under etc directory */
    if (vfs_add_child(etc, banner) != VFS_OK) {
        KLOG_ERROR("RAMFS",
                   "populate initial tree failed. failed to add child %s (type "
                   "%s) to parent %s (type %s)\n",
                   banner->name, vfs_get_node_type(banner->type), etc->name,
                   vfs_get_node_type(etc->type));
        return VFS_ERR_INVALID;
    }

    KLOG_INFO("RAMFS", "initial tree populated.\n");

    return VFS_OK;
}

const vfs_node_ops_t ramfs_file_ops = {
    .open = NULL,
    .read = ramfs_read,
    .write = ramfs_write,
};
