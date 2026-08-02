#include "tests/test_console.h"
#include "drivers/console_input.h"
#include "fs/vfs.h"

uint8_t console_buffer_input_test(void) {
    char buf[4];

    /* initialize console input */
    console_input_init();

    if (console_input_available() != 0) {
        KLOG_ERROR("CONSOLE_TEST", "buffer_input test failed. failed to "
                                   "initialize the console input.\n");
        return 0;
    }

    /* add a character to the console buffer */
    if (console_input_push('A') != VFS_OK) {
        KLOG_ERROR("CONSOLE_TEST", "buffer_intput test failed. failed to push "
                                   "character (A) to the console buffer.\n");
        return 0;
    }

    /* push second character to console buffer */
    if (console_input_push('B') != VFS_OK) {
        KLOG_ERROR("CONSOLE_TEST", "buffer_intput test failed. failed to push "
                                   "character (B) to the console buffer.\n");
        return 0;
    }

    /* check the count of characters */
    uint32_t console_char_count = console_input_available();
    if (console_char_count != 2) {
        KLOG_ERROR("CONSOLE_TEST",
                   "buffer_input test failed. character count in buffer. "
                   "found=%u, expected=2.\n",
                   console_char_count);
        return 0;
    }

    /* read 4 characters from the buffer */
    int console_char_read_count = console_input_read(buf, 4);
    if (console_char_read_count != 2) {
        KLOG_ERROR("CONSOLE_TEST",
                   "buffer_input test failed. character read count = %u, "
                   "expected = 2.\n",
                   console_char_read_count);
        return 0;
    }

    /* validate the buffer read */
    if (buf[0] != 'A' || buf[1] != 'B') {
        KLOG_ERROR("CONSOLE_TEST",
                   "buffer_input test failed. read mismatch, buffer read = %s, "
                   "expected = AB.\n",
                   buf);
        return 0;
    }

    KLOG_INFO("CONSOLE_TEST", "buffer_input test passed.\n");
    return 1;
}
