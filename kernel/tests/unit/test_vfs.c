#include "tests/test_vfs.h"
#include "fs/vfs.h"
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

uint8_t fs_vfs_add_child_test(void) {
    vfs_node_t *root;
    vfs_node_t *hello;
    vfs_node_t *dev;

    if (vfs_init() != VFS_OK) {
        KLOG_ERROR("VFS_TEST",
                   "vfs_add_child test failed. failed to initalize root.\n");
        return 0;
    }

    root = vfs_get_root();
    if (!root) {
        KLOG_ERROR("VFS_TEST",
                   "vfs_add_child test failed. ref. to root is NULL.\n");
        return 0;
    }

    /* create a node object for a file hello.txt */
    hello = vfs_create_node("hello.txt", VFS_NODE_FILE, NULL, NULL);
    if (!hello) {
        KLOG_ERROR("VFS_TEST",
                   "vfs_add_child test failed. failed to create hello file.\n");
        return 0;
    }

    /* create a node object for dir dev */
    dev = vfs_create_node("dev", VFS_NODE_DIR, NULL, NULL);
    if (!dev) {
        KLOG_ERROR("VFS_TEST",
                   "vfs_add_child test failed.  failed to create dir dev.\n");
        return 0;
    }

    /* add hello node object to the root */
    if (vfs_add_child(root, hello) != VFS_OK) {
        KLOG_ERROR("VFS_TEST",
                   "vfs_add_child test failed. failed to link child %s to "
                   "parent %s.\n",
                   hello->name, root->name);
        return 0;
    }

    /* add dev node object to the root */
    if (vfs_add_child(root, dev) != VFS_OK) {
        KLOG_ERROR("VFS_TEST",
                   "vfs_add_child test failed. failed to link child %s to "
                   "parent %s.\n",
                   dev->name, root->name);
        return 0;
    }

    /* check if hello properly linked with parent */
    if (hello->parent != root) {
        KLOG_ERROR("VFS_TEST",
                   "vfs_add_child test failed. parent of %s is %s, expected: / "
                   "(root).\n",
                   hello->name, hello->parent->name);
        return 0;
    }

    /* check if dev properly linked with parent */
    if (dev->parent != root) {
        KLOG_ERROR("VFS_TEST",
                   "vfs_add_child test failed. parent of %s is %s, expected: / "
                   "(root).\n",
                   dev->name, dev->parent->name);
        return 0;
    }

    /* check root as it should have child nodes now */
    if (root->first_child == NULL) {
        KLOG_ERROR("VFS_TEST", "vfs_add_child test failed. root doesn't have "
                               "children, expected: two children.\n");
        return 0;
    }

    /* test failure case to link direct to file should return error */
    int res = vfs_add_child(hello, dev);
    if (res != VFS_ERR_NOTDIR) {
        KLOG_ERROR("VFS",
                   "vfs_add_child test failed. add %s (dir) as a child to "
                   "parent %s (file) returned %u, expected %u.\n",
                   dev->name, hello->name, res, VFS_ERR_NOTDIR);
        return 0;
    }

    KLOG_INFO("VFS", "vfs_add_child test passed.\n");

    return 1;
}
