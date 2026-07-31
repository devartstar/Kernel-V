/*
 * syscalls.c - Syscall wrappers for freestanding C user-mode programs.
 *
 * Provides thin wrappers over the `int 0x80` syscall ABI so user programs do
 * not have to embed inline assembly. This translation unit is linked into
 * every C user program alongside utils/string.c. Declarations live in
 * utils/user_utils.h.
 *
 * Syscall ABI: eax = number, ebx/ecx/edx = args 1-3, result returned in eax.
 */
#include "user_utils.h"

/* Issue a syscall with three register arguments (ebx, ecx, edx). Unused
 * arguments are passed as 0. */
static inline int32_t usyscall(int32_t num, uint32_t a1, uint32_t a2,
                               uint32_t a3) {
    int32_t ret;
    __asm__ __volatile__("int $0x80"
                         : "=a"(ret)
                         : "a"(num), "b"(a1), "c"(a2), "d"(a3)
                         : "memory");
    return ret;
}

__attribute__((noreturn)) void uexit(int32_t code) {
    usyscall(SYS_EXIT, (uint32_t)code, 0, 0);
    for (;;) {
        /* SYS_EXIT does not return; spin defensively just in case. */
    }
}

int32_t uwrite(int fd, const void *buf, uint32_t len) {
    return usyscall(SYS_WRITE, (uint32_t)fd, (uint32_t)buf, len);
}

int32_t uread(int fd, void *buf, uint32_t len) {
    return usyscall(SYS_READ, (uint32_t)fd, (uint32_t)buf, len);
}

int32_t uopen(const char *path, uint32_t flags) {
    return usyscall(SYS_OPEN, (uint32_t)path, flags, 0);
}

int32_t uclose(int fd) {
    return usyscall(SYS_CLOSE, (uint32_t)fd, 0, 0);
}

int32_t ulseek(int fd, int32_t offset, int32_t whence) {
    return usyscall(SYS_LSEEK, (uint32_t)fd, (uint32_t)offset, (uint32_t)whence);
}

int32_t ugetpid(void) { return usyscall(SYS_GETPID, 0, 0, 0); }

int32_t uyield(void) { return usyscall(SYS_SCHED_YIELD, 0, 0, 0); }

int32_t uputs(const char *s) {
    return uwrite(STDOUT_FD, s, ustrlen(s));
}
