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
                     "offset %u exceeds file size %u.\n",
                     node->name, available, offset, file->size);
        to_copy = available;
    } else {
        to_copy = len;
    }

    /* copy the bytes to the buffer */
    ramfs_memcpy((uint8_t *)buf, file->data + offset, to_copy);

    return (int)to_copy;
}
