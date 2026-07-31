/*
 * userprog_stdin.c - C user-space program that reads a line from /dev/stdin
 * and echoes it back to /dev/stdout.
 *
 * Built as a freestanding, non-PIE flat binary loaded at 0x00400000 (see
 * user.ld) and embedded into the test kernel via objcopy. It runs in user
 * mode, issues real syscalls through the shared runtime in utils/user_utils.h,
 * and reports its verdict through SYS_EXIT:
 *
 *   exit 0 -> input was read from stdin and echoed back
 *   exit 1 -> stdin returned an unexpected error
 *
 * /dev/stdin is backed by the interactive console buffer: reads return
 * ERROR_AGAIN while the buffer is empty, so the program yields and retries
 * until a line of input arrives.
 */

#include <stddef.h>
#include <stdint.h>

#include "utils/user_utils.h"

/* Placed first in the binary (see user.ld) so the kernel entry at 0x00400000
 * lands on _start. */
__attribute__((section(".text.start"), used, noreturn)) void _start(void) {
    char buf[64];

    uputs("stdin-test: starting\n");
    uputs("stdin-test: type input: ");

    for (;;) {
        int32_t len = uread(STDIN_FD, buf, sizeof(buf));

        if (len > 0) {
            uputs("\nstdin-test: read bytes: ");
            uwrite(STDOUT_FD, buf, (uint32_t)len);
            uwrite(STDOUT_FD, "\n", 1);
            uputs("stdin-test: all checks passed\n");
            uexit(0);
        }

        /* No data available yet: yield and keep waiting for user input. */
        if (len != ERROR_AGAIN) {
            uputs("\nstdin-test: FAIL stdin error\n");
            uexit(1);
        }

        uyield();
    }
}
