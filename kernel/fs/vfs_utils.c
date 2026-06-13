#include "fs/vfs_utils.h"

const char *vfs_get_node_type(vfs_node_type_t type) {
    switch (type) {
    case VFS_NODE_FILE:
        return "FILE";
    case VFS_NODE_DIR:
        return "DIRECTORY";
    case VFS_NODE_CHARDEV:
        return "CHARACTER_DEVICE";
    default:
        return "INVALID_TYPE";
    }
    return "INVALID_TYPE";
}
