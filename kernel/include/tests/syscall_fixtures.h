#ifndef SYSCALL_FIXTURES_H
#define SYSCALL_FIXTURES_H

/* Shared by the kernel test setup and freestanding user test programs.
 * Each concurrent case owns one file AND one backing buffer. Never use a
 * shared writable fixture for a write/seek/read/verify sequence. */
#define SYSCALL_FIXTURE_DIR "syscall-tests"
#define SYSCALL_FIXTURE_PATH(name) "/" SYSCALL_FIXTURE_DIR "/" name
#define SYSCALL_OPEN_FIXTURE "open"
#define SYSCALL_READ_FIXTURE "read"
#define SYSCALL_STDIO_FIXTURE "stdio"
#define SYSCALL_RWS_FIXTURE "rws"
#define SYSCALL_FIXTURE_CAPACITY 64u
#define SYSCALL_FIXTURE_INITIAL "Hello from Kernel-V FS.\n"

#endif /* SYSCALL_FIXTURES_H */
