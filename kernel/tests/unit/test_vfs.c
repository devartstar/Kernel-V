#include "tests/test_vfs.h"
#include "fs/vfs.h"
#include "fs/vfs_root.h"
#include "fs/vfs_utils.h"

uint8_t fs_vfs_root_init_test(void) {
    vfs_node_t *root;

    /* initialize the vfs root node */
    if (vfs_init() != VFS_OK) {
        KLOG_ERROR("VFS_TEST", "vfs_root_init test failed. vfs_init failed.\n");
        return 0;
    }

    /* get the vfs root initialized */
    root = vfs_get_root();
    if (!root) {
        KLOG_ERROR("VFS_TEST",
                   "vfs_root_init test failed. invalid ref. (%p) to the root "
                   "node object.\n",
                   root);
        return 0;
    }

    /* check for valid root name */
    if (root->name[0] != '/' || root->name[1] != '\0') {
        KLOG_ERROR("VFS_TEST",
                   "vfs_root_init test failed. invalid root name = %s, "
                   "expected = /.\n",
                   root->name);
        return 0;
    }

    /* check for root node type should be dir */
    if (root->type != VFS_NODE_DIR) {
        KLOG_ERROR("VFS_TEST",
                   "vfs_root_init test failed. invalid root type = %s, "
                   "expected = DIRECTORY.\n",
                   vfs_get_node_type(root->type));
        return 0;
    }

    /* root should not have any parent node */
    if (root->parent != NULL) {
        KLOG_ERROR("VFS_TEST",
                   "vfs_root_init test failed. root parent found at ref %p, "
                   "should be NULL.\n",
                   root->parent);
        return 0;
    }

    /* root should not have any children just after init */
    if (root->first_child != NULL) {
        KLOG_ERROR("VFS_TEST",
                   "vfs_root_init test failed. root has first child at ref %p, "
                   "should be have any child just post init.\n",
                   root->first_child);
        return 0;
    }

    KLOG_INFO("VFS_TEST",
              "vfs_root_init succeeded. Root initalized successfully.\n");
    return 1;
}
