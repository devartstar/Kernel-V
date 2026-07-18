/*
 * userprog_write.c - C user spacr program that tests SYSCALL_WRITE syscall.
 *
 * Built as a freestanding, non-PIE flat binary loaded at 0x00400000 (see
 * user.ld) and embedded into test kernel via objcopy. It runs in user mode,
 * issues real syscall through `int 0x80`, and reports vertict through SYS_EXIT
 *
 * exit 0 -> every check passed
 *
 */

#include <stddef.h>
#include <stdint.h>

#define SYS_EXIT 1
#define SYS_WRITE 2
#define SYS_GETPID 3
#define SYS_SCHED_YIELD 4
#define SYS_OPEN 5
#define SYS_READ 6

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2

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

static __attribute__((noreturn)) void uexit(int32_t code) {
    do_syscall(SYS_EXIT, (uint32_t)code, 0, 0);
    for (;;) {
        /* SYS_EXIT does not return; spin defensively just in case. */
    }
}

__attribute__((section(".text.start"), used, noreturn)) void _start(void) {
    int ret;

    char *msg = "syscall_write test starting.\n";
    ret = uwrite(STDOUT_FD, msg, ustrlen(msg));
    if (ret < 0) {
        uexit(ret);
    }

    /* Print a messgae for 10 times */
    msg = "hello from usermode!\n";
    for (int i = 1; i <= 10; i++) {
        ret = uwrite(STDOUT_FD, msg, ustrlen(msg));
        if (ret < 0) {
            uexit(i);
        }
    }

    msg = "syscall_write test completed.\n";
    ret = uwrite(STDOUT_FD, msg, ustrlen(msg));
    if (ret < 0) {
        uexit(ret);
    }

    uexit(0);
}
