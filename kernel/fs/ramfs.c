#include "fs/ramfs.h"
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

int ramfs_write(vfs_node_t *node, uint32_t offset, void *buf, uint32_t len) {
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

const vfs_node_ops_t ramfs_file_ops = {
    .open = NULL,
    .read = ramfs_read,
    .write = ramfs_write,
};
