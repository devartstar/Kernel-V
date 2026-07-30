/*
 * userprog_stdio.c - C user space program that tests the standard I/O
 * syscalls (SYS_WRITE / SYS_READ) against the std streams and a regular file.
 *
 * Built as a freestanding, non-PIE flat binary loaded at 0x00400000 (see
 * user.ld) and embedded into test kernel via objcopy. It runs in user mode,
 * issues real syscall through `int 0x80`, and reports vertict through SYS_EXIT
 *
 * NOTE:: none of the std io files supports open.
 * /dev/stdin - supports read, doesnt support write
 * /dev/stdout - doesnt support read, supports write
 * /dev/stderr - doesnt support read, supports write
 *
 * exit 0 -> every check passed
 * exit 1 -> failed to write to stdout
 * exit 2 -> failed to write to stderr
 * exit 3 -> read returned unexpected code, and write to stderr failed.
 * exit 4 -> read returned unexpected code, and write to stderr success.
 * exit 5 -> failed to open standard file
 * exit 6 -> failed to read from a standard file
 * exit 7 -> failed to close a standard file
 * exit 8 -> failed to seek within the file
 * exit 9 -> failed to write to the file
 * exit 10 -> failed to read from serial stdin to stdout
 *
 */

#include <stddef.h>
#include <stdint.h>

#include "user_string.h"

#define SYS_EXIT 1
#define SYS_WRITE 2
#define SYS_GETPID 3
#define SYS_SCHED_YIELD 4
#define SYS_OPEN 5
#define SYS_READ 6
#define SYS_CLOSE 7
#define SYS_LSEEK 8

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2
#define FIRST_NORMAL_FD 3

#define SEEK_SET 0

#define ERROR_NOOP -4
#define ERROR_AGAIN -6 /* buffer exists but empty */

static uint32_t ustrlen(const char *s) {
    uint32_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

/* here to make syscall interrupt */
static inline int32_t do_syscall(int32_t num, uint32_t a1, uint32_t a2,
                                 uint32_t a3) {
    int32_t ret;
    __asm__ __volatile__("int $0x80"
                         : "=a"(ret)
                         : "a"(num), "b"(a1), "c"(a2), "d"(a3)
                         : "memory");
    return ret;
}

static int32_t uwrite(const int fd, const char *buf, const uint32_t len) {
    return do_syscall(SYS_WRITE, fd, (uint32_t)buf, len);
}

static int32_t uread(const int fd, const char *buf, const uint32_t len) {
    return do_syscall(SYS_READ, fd, (uint32_t)buf, len);
}

static int32_t uopen(const char *path, uint32_t flags) {
    return do_syscall(SYS_OPEN, (uint32_t)path, flags, 0);
}

static int32_t uclose(const uint32_t fd) {
    return do_syscall(SYS_CLOSE, fd, 0, 0);
}

static int32_t ulseek(const int fd, const int32_t offset,
                      const int32_t whence) {
    return do_syscall(SYS_LSEEK, (uint32_t)fd, (uint32_t)offset,
                      (uint32_t)whence);
}

static int32_t uyield() { return do_syscall(SYS_SCHED_YIELD, 0, 0, 0); }

static __attribute__((noreturn)) void uexit(int32_t code) {
    do_syscall(SYS_EXIT, (uint32_t)code, 0, 0);
    for (;;) {
        /* SYS_EXIT does not return; spin defensively just in case. */
    }
}

__attribute__((section(".text.start"), used, noreturn)) void _start(void) {
    int ret;
    char buf[8];

    char *msg = "syscall_stdio test starting.\n";
    ret = uwrite(STDOUT_FD, msg, ustrlen(msg));
    if (ret < 0) {
        uexit(1);
    }

    /* Case 1: Write a messgae to stdout file */
    msg = "hello /dev/stdout from usermode!\n";
    ret = uwrite(STDOUT_FD, msg, ustrlen(msg));
    if (ret < 0) {
        uexit(1);
    }

    /* Case 2: Write a message to stderr file */
    msg = "hello /dev/stderr from usermode!\n";
    ret = uwrite(STDERR_FD, msg, ustrlen(msg));
    if (ret < 0) {
        uexit(2);
    }

    /* Case 3: Try reading from a stdin file
     * stdin file backing console buffer is empty will return ERROR_AGAIN
     */
    ret = uread(STDIN_FD, buf, sizeof(buf));
    if (ret != ERROR_AGAIN) {
        msg = "read failed: expected noop.\n";
        ret = uwrite(STDERR_FD, msg, ustrlen(msg));
        if (ret < 0) {
            uexit(3);
        } else {
            uexit(4);
        }
    } else {
        msg = "read returned noop(expected).\n";
        uwrite(STDOUT_FD, msg, ustrlen(msg));
    }

    /* Case 4: write a known pattern to a file, read it back, echo to stdout.
     * (/dev/stdin cannot be written to, so the readback is echoed to stdout.)
     */
    int32_t fd = uopen("/hello.txt", 0);
    if (fd < FIRST_NORMAL_FD) {
        msg = "failed open existing file.\n";
        uwrite(STDERR_FD, msg, ustrlen(msg));
        uexit(5);
    }

    const char *known = "KNOWN";
    uint32_t known_len = ustrlen(known);

    /* rewind and write the known pattern */
    if (ulseek(fd, 0, SEEK_SET) < 0) {
        msg = "failed to seek /hello.txt before write.\n";
        uwrite(STDERR_FD, msg, ustrlen(msg));
        uexit(8);
    }
    ret = uwrite(fd, known, known_len);
    if (ret != (int)known_len) {
        msg = "failed writing content to file /hello.txt.\n";
        uwrite(STDERR_FD, msg, ustrlen(msg));
        uexit(9);
    }

    /* rewind and read the same content back */
    if (ulseek(fd, 0, SEEK_SET) < 0) {
        msg = "failed to seek /hello.txt before read.\n";
        uwrite(STDERR_FD, msg, ustrlen(msg));
        uexit(8);
    }
    ret = uread(fd, buf, known_len);
    if (ret != (int)known_len) {
        msg = "failed reading content from file /hello.txt.\n";
        uwrite(STDERR_FD, msg, ustrlen(msg));
        uexit(6);
    }

    msg = "successfully wrote and read back the known pattern.\n";
    uwrite(STDOUT_FD, msg, ustrlen(msg));

    buf[known_len] = '\0';
    char line[64];
    int line_len = usnprintf(line, sizeof(line), "file reads: %s.\n", buf);
    ret = uwrite(STDOUT_FD, line, (uint32_t)line_len);
    if (ret < 0) {
        msg = "failed to write read contents from /hello.txt to /dev/stdout.\n";
        uwrite(STDERR_FD, msg, ustrlen(msg));
    }

    msg = "\ntype input: ";
    ret = uwrite(STDOUT_FD, msg, ustrlen(msg));
    if (ret < 0) {
        msg = "failed writing msg: \n";
        uwrite(STDERR_FD, msg, ustrlen(msg));
        uexit(1);
    }

    int len;
    while (1) {
        len = uread(STDIN_FD, buf, sizeof(buf));

        if (len > 0) {
            msg = "\nread bytes: ";
            uwrite(STDOUT_FD, msg, ustrlen(msg));
            uwrite(STDOUT_FD, buf, len);
            uwrite(STDOUT_FD, "\n", 1);
            uexit(0);
        }

        /* no data available yet: yield and keep waiting for user input */
        if (len != ERROR_AGAIN) {
            msg = "\nstdin error\n";
            uwrite(STDOUT_FD, msg, ustrlen(msg));
            uexit(10);
        }

        uyield();
    }

    ret = uclose(fd);
    if (ret < 0) {
        msg = "failed to close /hello.txt.\n";
        uwrite(STDERR_FD, msg, ustrlen(msg));
        uexit(8);
    }

    msg = "syscall_stdio test completed.\n";
    ret = uwrite(STDOUT_FD, msg, ustrlen(msg));
    if (ret < 0) {
        uexit(1);
    }

    uexit(0);
}
