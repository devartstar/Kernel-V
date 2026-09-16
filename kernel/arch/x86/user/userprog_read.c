/*
 * userprog_read.c - C user-space program that tests the SYS_READ syscall.
 *
 * Built as a freestanding, non-PIE flat binary loaded at 0x00400000 (see
 * user.ld) and embedded into the test kernel via objcopy. It runs in user
 * mode, issues real syscalls through `int 0x80`, and reports its verdict
 * through SYS_EXIT:
 *
 *   exit 0 -> every check passed
 *   exit 1 -> failure to open a file.
 *   exit 2 -> read file contents returned error
 *   exit 3 -> read file length mismatch
 *   exit 4 -> read file false passed when buffer is invalid.
 *
 * The kernel-side harness (syscall_itest.c) seeds the VFS with the fixture
 * file, spawns this blob, and asserts that it terminates with exit code 0.
 */

#include <stddef.h>
#include <stdint.h>

#include "utils/user_utils.h"

/* Placed first in the binary (see user.ld) so the kernel entry at 0x00400000
 * lands on _start. */
__attribute__((section(".text.start"), used, noreturn)) void _start(void) {
    uputs("read-test: starting\n");

    /* Step 1: opening an existing file yields a valid descriptor (>= 3). */
    int32_t fd = uopen("/hello.txt", 0);
    if (fd < FIRST_NORMAL_FD) {
        uputs("read-test: FAIL open existing file\n");
        uexit(1);
    }

    /* Step 2: read the contents of the open file */
    char buf[16];
    int32_t read_len = uread(fd, buf, 5);
    if (read_len < 0) {
        uputs("read-test: FAIL reading file contents.\n");
        uexit(2);
    }

    /* Step 3: verify the length of the read contents */
    if (read_len != 5) {
        uputs("read-test: FAIL, read length mismatch.\n");
        uexit(3);
    }

    /* Step 4: write to thte file */
    uwrite(fd, "Hello World", 11);

    /* Step 4: check negative case when buffer is invalid */
    read_len = uread(fd, NULL, 5);
    if (read_len >= 0) {
        uputs("read-test: FAIL, returnd success, expected error.\n");
        uexit(4);
    }

    uputs("read-test: all checks passed\n");
    uexit(0);
}
