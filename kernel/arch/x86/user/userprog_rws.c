/*
 * userprog_rws.c - C user-space program that exercises the SYS_READ,
 * SYS_WRITE and SYS_LSEEK syscalls across their variations.
 *
 * Built as a freestanding, non-PIE flat binary loaded at 0x00400000 (see
 * user.ld) and embedded into the test kernel via objcopy. It runs in user
 * mode, issues real syscalls through `int 0x80`, and reports its verdict
 * through SYS_EXIT (exit 0 == every check passed).
 *
 * The test is self-contained: it rewrites the fixture file with a known
 * pattern before validating reads/seeks, so it does not depend on the
 * original fixture contents (other syscall tests may mutate it).
 *
 * Exit codes (each distinct for diagnosability):
 *   0  -> every check passed
 *   1  -> open existing file did not return a valid fd
 *   2  -> lseek(SET, 0) before write failed
 *   3  -> write of known pattern did not write the full length
 *   4  -> lseek(SET, 0) before readback failed
 *   5  -> readback returned an error
 *   6  -> readback length mismatch
 *   7  -> readback content mismatch
 *   8  -> lseek(CUR) failed or landed at the wrong offset
 *   9  -> read after SEEK_CUR content mismatch
 *   10 -> lseek(SET, mid) failed
 *   11 -> read after SEEK_SET(mid) content mismatch
 *   12 -> lseek(END, 0) failed
 *   13 -> read at end-of-file did not return 0
 *   14 -> lseek(END, -1) failed or read of last byte failed
 *   15 -> lseek(SET, negative) was not rejected
 *   16 -> lseek(CUR, negative-underflow) was not rejected
 *   17 -> lseek with invalid whence was not rejected
 *   18 -> write with NULL buffer was not rejected
 *   19 -> read with NULL buffer was not rejected
 *   20 -> write beyond file capacity was not rejected
 *   21 -> close of a valid fd failed
 *   22 -> read on a closed fd was not rejected
 *   23 -> lseek on a closed fd was not rejected
 */

#include <stddef.h>
#include <stdint.h>

#define SYS_EXIT 1
#define SYS_WRITE 2
#define SYS_GETPID 3
#define SYS_SCHED_YIELD 4
#define SYS_OPEN 5
#define SYS_READ 6
#define SYS_CLOSE 7
#define SYS_LSEEK 8

/* whence values (mirror VFS_SEEK_* in the kernel). */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* stdout descriptor and the lowest descriptor a successful open can return. */
#define STDOUT_FD 1
#define FIRST_NORMAL_FD 3

#define FIXTURE_PATH "/hello.txt"

/* Known pattern written to the fixture before validation. */
static const char PATTERN[] = "ABCDEFGHIJ";
#define PATTERN_LEN 10

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

/* Compare n bytes; return 1 if equal, 0 otherwise. */
static int umemeq(const char *a, const char *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static int32_t uopen(const char *path) {
    return do_syscall(SYS_OPEN, (uint32_t)path, 0, 0);
}

static int32_t uread(int fd, void *buf, uint32_t len) {
    return do_syscall(SYS_READ, (uint32_t)fd, (uint32_t)buf, len);
}

static int32_t uwrite(int fd, const void *buf, uint32_t len) {
    return do_syscall(SYS_WRITE, (uint32_t)fd, (uint32_t)buf, len);
}

static int32_t ulseek(int fd, int32_t offset, int32_t whence) {
    return do_syscall(SYS_LSEEK, (uint32_t)fd, (uint32_t)offset,
                      (uint32_t)whence);
}

static int32_t uclose(int fd) {
    return do_syscall(SYS_CLOSE, (uint32_t)fd, 0, 0);
}

static int32_t uwrite_stdout(const char *s) {
    return do_syscall(SYS_WRITE, STDOUT_FD, (uint32_t)s, ustrlen(s));
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
    char buf[32];

    uwrite_stdout("rws-test: starting");

    /* Step 0: open the fixture file. */
    int32_t fd = uopen(FIXTURE_PATH);
    if (fd < FIRST_NORMAL_FD) {
        uwrite_stdout("rws-test: FAIL open fixture");
        uexit(1);
    }

    /* Step 1: rewind and lay down a known pattern so the rest of the test is
     * independent of the fixture's prior contents. */
    if (ulseek(fd, 0, SEEK_SET) != 0) {
        uwrite_stdout("rws-test: FAIL lseek SET before write");
        uexit(2);
    }

    int32_t wrote = uwrite(fd, PATTERN, PATTERN_LEN);
    if (wrote != PATTERN_LEN) {
        uwrite_stdout("rws-test: FAIL write pattern");
        uexit(3);
    }

    /* Step 2: rewind with SEEK_SET and read the whole pattern back. */
    if (ulseek(fd, 0, SEEK_SET) != 0) {
        uwrite_stdout("rws-test: FAIL lseek SET before read");
        uexit(4);
    }

    int32_t got = uread(fd, buf, PATTERN_LEN);
    if (got < 0) {
        uwrite_stdout("rws-test: FAIL readback error");
        uexit(5);
    }
    if (got != PATTERN_LEN) {
        uwrite_stdout("rws-test: FAIL readback length");
        uexit(6);
    }
    if (!umemeq(buf, PATTERN, PATTERN_LEN)) {
        uwrite_stdout("rws-test: FAIL readback content");
        uexit(7);
    }

    /* Step 3: SEEK_CUR - after reading PATTERN_LEN bytes the offset is at
     * PATTERN_LEN; rewind 5 bytes and confirm we land on 'F'. */
    if (ulseek(fd, -5, SEEK_CUR) != (PATTERN_LEN - 5)) {
        uwrite_stdout("rws-test: FAIL lseek CUR");
        uexit(8);
    }
    got = uread(fd, buf, 1);
    if (got != 1 || buf[0] != 'F') {
        uwrite_stdout("rws-test: FAIL read after SEEK_CUR");
        uexit(9);
    }

    /* Step 4: SEEK_SET to a middle offset and read a few bytes ("CDE"). */
    if (ulseek(fd, 2, SEEK_SET) != 2) {
        uwrite_stdout("rws-test: FAIL lseek SET mid");
        uexit(10);
    }
    got = uread(fd, buf, 3);
    if (got != 3 || !umemeq(buf, "CDE", 3)) {
        uwrite_stdout("rws-test: FAIL read after SEEK_SET mid");
        uexit(11);
    }

    /* Step 5: SEEK_END(0) returns the file size; reading there yields EOF. */
    int32_t end = ulseek(fd, 0, SEEK_END);
    if (end < PATTERN_LEN) {
        uwrite_stdout("rws-test: FAIL lseek END");
        uexit(12);
    }
    got = uread(fd, buf, 4);
    if (got != 0) {
        uwrite_stdout("rws-test: FAIL read at EOF");
        uexit(13);
    }

    /* Step 6: SEEK_END(-1) points at the final byte; a 1-byte read succeeds. */
    if (ulseek(fd, -1, SEEK_END) != (end - 1)) {
        uwrite_stdout("rws-test: FAIL lseek END-1");
        uexit(14);
    }
    got = uread(fd, buf, 1);
    if (got != 1) {
        uwrite_stdout("rws-test: FAIL read last byte");
        uexit(14);
    }

    /* Step 7: seeking before the start of the file is rejected. */
    if (ulseek(fd, -1000, SEEK_SET) >= 0) {
        uwrite_stdout("rws-test: FAIL lseek SET negative accepted");
        uexit(15);
    }

    /* Step 8: SEEK_CUR that underflows below zero is rejected. */
    if (ulseek(fd, 0, SEEK_SET) != 0) {
        uexit(16);
    }
    if (ulseek(fd, -1, SEEK_CUR) >= 0) {
        uwrite_stdout("rws-test: FAIL lseek CUR underflow accepted");
        uexit(16);
    }

    /* Step 9: an unknown whence value is rejected. */
    if (ulseek(fd, 0, 99) >= 0) {
        uwrite_stdout("rws-test: FAIL lseek invalid whence accepted");
        uexit(17);
    }

    /* Step 10: writing from a NULL buffer is rejected. */
    if (uwrite(fd, NULL, 4) >= 0) {
        uwrite_stdout("rws-test: FAIL write NULL buffer accepted");
        uexit(18);
    }

    /* Step 11: reading into a NULL buffer is rejected. */
    if (uread(fd, NULL, 4) >= 0) {
        uwrite_stdout("rws-test: FAIL read NULL buffer accepted");
        uexit(19);
    }

    /* Step 12: a write that exceeds the file capacity must not report a full
     * write (the ramfs backing store is 64 bytes). */
    char big[100];
    for (uint32_t i = 0; i < sizeof(big); i++) {
        big[i] = 'x';
    }
    if (ulseek(fd, 0, SEEK_SET) != 0) {
        uexit(20);
    }
    if (uwrite(fd, big, sizeof(big)) == (int32_t)sizeof(big)) {
        uwrite_stdout("rws-test: FAIL write beyond capacity accepted");
        uexit(20);
    }

    /* Step 13: closing a valid descriptor succeeds. */
    if (uclose(fd) < 0) {
        uwrite_stdout("rws-test: FAIL close");
        uexit(21);
    }

    /* Step 14: operations on a closed descriptor are rejected. */
    if (uread(fd, buf, 1) >= 0) {
        uwrite_stdout("rws-test: FAIL read after close accepted");
        uexit(22);
    }
    if (ulseek(fd, 0, SEEK_SET) >= 0) {
        uwrite_stdout("rws-test: FAIL lseek after close accepted");
        uexit(23);
    }

    uwrite_stdout("rws-test: all checks passed");
    uexit(0);
}
