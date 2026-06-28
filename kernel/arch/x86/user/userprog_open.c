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

#define SYS_EXIT 1
#define SYS_WRITE 2
#define SYS_GETPID 3
#define SYS_SCHED_YIELD 4
#define SYS_OPEN 5

/* stdout descriptor and the lowest descriptor a successful open can return. */
#define STDOUT_FD 1
#define FIRST_NORMAL_FD 3

/* Issue a syscall with three register arguments (ebx, ecx, edx). Unused
 * arguments are passed as 0. The kernel maps eax=number, ebx=arg1, ecx=arg2,
 * edx=arg3. */
static inline int32_t do_syscall(int32_t num, uint32_t a1, uint32_t a2,
                                 uint32_t a3) {
    int32_t ret;
    __asm__ __volatile__("int $0x80"
                         : "=a"(ret)
                         : "a"(num), "b"(a1), "c"(a2), "d"(a3)
                         : "memory");
    return ret;
}

static uint32_t ustrlen(const char *s) {
    uint32_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static void uwrite(const char *s) {
    do_syscall(SYS_WRITE, STDOUT_FD, (uint32_t)s, ustrlen(s));
}

static __attribute__((noreturn)) void uexit(int32_t code) {
    do_syscall(SYS_EXIT, (uint32_t)code, 0, 0);
    for (;;) {
        /* SYS_EXIT does not return; spin defensively just in case. */
    }
}

/* Placed first in the binary (see user.ld) so the kernel entry at 0x00400000
 * lands on _start. */
__attribute__((section(".text.start"), used, noreturn)) void _start(void) {
    uwrite("open-test: starting\n");

    /* Check 1: opening an existing file yields a valid descriptor (>= 3). */
    int32_t fd = do_syscall(SYS_OPEN, (uint32_t) "/hello.txt", 0, 0);
    if (fd < FIRST_NORMAL_FD) {
        uwrite("open-test: FAIL open existing file\n");
        uexit(1);
    }

    /* Check 2: opening a missing file is rejected with a negative error. */
    int32_t missing = do_syscall(SYS_OPEN, (uint32_t) "/no_such_file.txt", 0, 0);
    if (missing >= 0) {
        uwrite("open-test: FAIL open missing file\n");
        uexit(2);
    }

    uwrite("open-test: all checks passed\n");
    uexit(0);
}
