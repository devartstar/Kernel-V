#ifndef SYSCALL_ITEST_H
#define SYSCALL_ITEST_H

#include <stdint.h>

/* Maximum number of cases a single run can track. */
#define SYSCALL_ITEST_MAX 8

/*
 * Describes one syscall integration test case.
 *
 * A case is an embedded user program (a flat binary blob linked into the test
 * kernel via objcopy) that exercises one or more syscalls and reports its
 * verdict through its SYS_EXIT code. A case PASSES when the spawned user
 * process terminates with exit_code == expected_exit_code within the timeout.
 *
 * Convention for self-checking user programs:
 *   exit code 0    -> every in-process syscall check passed
 *   exit code != 0 -> identifies which check failed (program-defined)
 */
typedef struct {
    const char *name;           /* human-readable case name (also proc name) */
    const uint8_t *blob_start;  /* linker symbol: start of embedded blob */
    const uint8_t *blob_end;    /* linker symbol: end of embedded blob */
    int32_t expected_exit_code; /* exit code that counts as success */
} syscall_itest_case_t;

/*
 * syscall_itest_run_all - spawn every case as a user process, wait for them all
 * to terminate, then verify each one's exit code against its expectation.
 *
 * Must be called from a schedulable kernel-thread context (e.g. kernel_main):
 * it parks on `hlt` and relies on the timer/scheduler to run the spawned user
 * processes to completion.
 *
 * @cases - array of test cases
 * @count - number of cases (capped at SYSCALL_ITEST_MAX)
 *
 * @return number of FAILED cases (0 means all passed)
 */
uint32_t syscall_itest_run_all(const syscall_itest_case_t *cases,
                               uint32_t count);

#endif /* SYSCALL_ITEST_H */
