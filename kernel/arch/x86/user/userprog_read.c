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
 *   exit 5 -> initial content mismatch
 *   exit 6 -> write failed
 *   exit 7 -> seek/readback mismatch
 *
 * The kernel-side setup (userproc_tests.c) seeds a private fixture
 * file, spawns this blob, and asserts that it terminates with exit code 0.
 */

#include <stddef.h>
#include <stdint.h>

#include "tests/syscall_fixtures.h"
#include "utils/user_utils.h"

/* Placed first in the binary (see user.ld) so the kernel entry at 0x00400000
 * lands on _start. */
__attribute__((section(".text.start"), used, noreturn)) void _start(void) {
    uputs("read-test: starting\n");

    /* Step 1: opening an existing file yields a valid descriptor (>= 3). */
    int32_t fd = uopen(SYSCALL_FIXTURE_PATH(SYSCALL_READ_FIXTURE), 0);
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

    if (!umemeq(buf, SYSCALL_FIXTURE_INITIAL, 5)) {
        uputs("read-test: FAIL initial content\n");
        uexit(5);
    }

    /* Write at the current offset, allow other cases to run, and verify the
     * bytes in this case's private fixture. */
    if (uwrite(fd, "Hello World", 11) != 11) {
        uputs("read-test: FAIL write\n");
        uexit(6);
    }
    uyield();
    if (ulseek(fd, 5, SEEK_SET) != 5 || uread(fd, buf, 11) != 11 ||
        !umemeq(buf, "Hello World", 11)) {
        uputs("read-test: FAIL readback\n");
        uexit(7);
    }

    /* Check negative case when buffer is invalid. */
    read_len = uread(fd, NULL, 5);
    if (read_len >= 0) {
        uputs("read-test: FAIL, returnd success, expected error.\n");
        uexit(4);
    }

    uputs("read-test: all checks passed\n");
    uexit(0);
}
