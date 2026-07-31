/*
 * user_utils.h - Shared freestanding runtime for C user-mode programs.
 *
 * User programs are built as standalone, non-PIE flat binaries loaded at
 * 0x00400000 (see user.ld). They are NOT linked against the kernel or any
 * libc, so every helper they need must be self contained. This header is the
 * single stitching point for the user runtime: it declares the syscall
 * wrappers (implemented in utils/syscalls.c) and the string/formatting
 * helpers (implemented in utils/string.c), which are linked into each
 * program's flat binary.
 *
 * Include with:  #include "utils/user_utils.h"
 * (GCC resolves quoted includes relative to the including file's directory,
 *  so no -I flag is required under USER_CFLAGS.)
 */
#ifndef USER_UTILS_H
#define USER_UTILS_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* ---- Syscall numbers (mirror proc/syscall.h in the kernel) ---- */
#define SYS_EXIT 1
#define SYS_WRITE 2
#define SYS_GETPID 3
#define SYS_SCHED_YIELD 4
#define SYS_OPEN 5
#define SYS_READ 6
#define SYS_CLOSE 7
#define SYS_LSEEK 8

/* ---- Standard file descriptors ---- */
#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2
#define FIRST_NORMAL_FD 3

/* ---- lseek whence values (mirror VFS_SEEK_* in the kernel) ---- */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* ---- Selected VFS error codes surfaced to user mode ---- */
#define ERROR_NOOP -4
#define ERROR_AGAIN -6 /* buffer exists but is currently empty */

/* ============================================================================
 * Syscall wrappers (implemented in utils/syscalls.c)
 *
 * Each wrapper issues `int 0x80` with eax=number and ebx/ecx/edx = args and
 * returns the kernel's result in eax. Negative values are VFS-style errors.
 * ==========================================================================*/

/** uexit - terminate the current process with @code. Does not return. */
__attribute__((noreturn)) void uexit(int32_t code);

/** uwrite - write @len bytes from @buf to descriptor @fd. */
int32_t uwrite(int fd, const void *buf, uint32_t len);

/** uread - read up to @len bytes from descriptor @fd into @buf. */
int32_t uread(int fd, void *buf, uint32_t len);

/** uopen - open @path with @flags and return a descriptor (>= 3) or error. */
int32_t uopen(const char *path, uint32_t flags);

/** uclose - close descriptor @fd. */
int32_t uclose(int fd);

/** ulseek - reposition @fd by @offset relative to @whence (SEEK_*). */
int32_t ulseek(int fd, int32_t offset, int32_t whence);

/** ugetpid - return the current process id. */
int32_t ugetpid(void);

/** uyield - voluntarily yield the CPU to the scheduler. */
int32_t uyield(void);

/** uputs - write a NUL-terminated string to stdout. */
int32_t uputs(const char *s);

/* ============================================================================
 * String / formatting helpers (implemented in utils/string.c)
 * ==========================================================================*/

/** ustrlen - length of a NUL-terminated string, excluding the terminator. */
uint32_t ustrlen(const char *s);

/** umemeq - return 1 if the first @n bytes of @a and @b are equal, else 0. */
int umemeq(const void *a, const void *b, uint32_t n);

/**
 * uvsnprintf - format into @buf using @fmt and a va_list.
 * @buf  - destination buffer (always NUL-terminated when size > 0)
 * @size - capacity of @buf in bytes
 * @fmt  - printf-style format (%s %c %d %u %x %p %%, optional 'l'/width)
 * @args - variadic arguments
 * @return number of characters written (excluding the NUL terminator).
 */
int uvsnprintf(char *buf, size_t size, const char *fmt, va_list args);

/** usnprintf - printf-style formatter into a fixed buffer. See uvsnprintf. */
int usnprintf(char *buf, size_t size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#endif /* USER_UTILS_H */
