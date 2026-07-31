/*
 * userprog_open.c - C user-space program that tests the SYS_OPEN syscall.
 *
 * Built as a freestanding, non-PIE flat binary loaded at 0x00400000 (see
 * user.ld) and embedded into the test kernel via objcopy. It runs in user
 * mode, issues real syscalls through `int 0x80`, and reports its verdict
 * through SYS_EXIT:
 *
 *   exit 0 -> every check passed
 *   exit 1 -> opening an existing file did not return a valid fd
 *   exit 2 -> opening a missing file did not return an error
 *
 * The kernel-side harness (syscall_itest.c) seeds the VFS with the fixture
 * file, spawns this blob, and asserts that it terminates with exit code 0.
 */

#include <stdint.h>

#include "utils/user_utils.h"

/* Placed first in the binary (see user.ld) so the kernel entry at 0x00400000
 * lands on _start. */
__attribute__((section(".text.start"), used, noreturn)) void _start(void) {
    uputs("open-test: starting\n");

    /* Check 1: opening an existing file yields a valid descriptor (>= 3). */
    int32_t fd = uopen("/hello.txt", 0);
    if (fd < FIRST_NORMAL_FD) {
        uputs("open-test: FAIL open existing file\n");
        uexit(1);
    }

    /* Check 2: opening a missing file is rejected with a negative error. */
    int32_t missing = uopen("/no_such_file.txt", 0);
    if (missing >= 0) {
        uputs("open-test: FAIL open missing file\n");
        uexit(2);
    }

    uputs("open-test: all checks passed\n");
    uexit(0);
}
