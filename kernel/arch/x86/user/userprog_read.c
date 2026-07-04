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

#define SYS_EXIT 1
#define SYS_WRITE 2
#define SYS_GETPID 3
#define SYS_SCHED_YIELD 4
#define SYS_OPEN 5
#define SYS_READ 6

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

static int32_t uread_file(const int fd, const char *buf, const uint32_t len) {
    return do_syscall(SYS_READ, fd, (uint32_t)buf, len);
}

static void uwrite_file(const int fd, const char *buf, const uint32_t len) {
    do_syscall(SYS_WRITE, fd, (uint32_t)buf, len);
}

static void uwrite_stdout(const char *s) {
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
    uwrite_stdout("read-test: starting\n");

    /* Step 1: opening an existing file yields a valid descriptor (>= 3). */
    int32_t fd = do_syscall(SYS_OPEN, (uint32_t) "/hello.txt", 0, 0);
    if (fd < FIRST_NORMAL_FD) {
        uwrite_stdout("read-test: FAIL open existing file\n");
        uexit(1);
    }

    /* Step 2: read the contents of the open file */
    char buf[16];
    int32_t read_len = uread_file(fd, buf, 5);
    if (read_len < 0) {
        uwrite_stdout("read-test: FAIL reading file contents.\n");
        uexit(2);
    }

    /* Step 3: verify the length of the read contents */
    if (read_len != 5) {
        uwrite_stdout("read-test: FAIL, read length mismatch.\n");
        uexit(3);
    }

    /* Step 4: check negative case when buffer is invalid */
    read_len = uread_file(fd, NULL, 5);
    if (read_len >= 0) {
        uwrite_stdout("read-test: FAIL, returnd success, expected error.\n");
        uexit(4);
    }

    uwrite_stdout("read-test: all checks passed\n");
    uexit(0);
}
