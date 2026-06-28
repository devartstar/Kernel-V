#include "tests/syscall_itest.h"
#include "lib/printk.h"
#include "proc/proc.h"
#include "proc/user.h"
#include "time/timer.h"

/*
 * Generic syscall integration-test harness.
 *
 * The flow mirrors the proven kernel-process test pattern (see
 * kernelproc_tests.c): spawn the work, then park the calling kernel thread on
 * `hlt` while the timer-driven scheduler runs the spawned processes. The
 * difference here is that each spawned process is a *user* process that makes
 * real syscalls and reports its verdict through SYS_EXIT.
 *
 * Verification reads the terminated process's exit_code. Because the idle
 * thread reclaims terminated PCBs (see cleanup_terminated_processes()), the
 * harness snapshots each exit code the first time it observes the process
 * terminated and guards against PCB reuse with a pid check.
 */

/* Upper bound on how long to wait for all processes to terminate before
 * declaring the stragglers timed-out failures. Expressed in timer ticks. */
#define SYSCALL_ITEST_TIMEOUT_TICKS 1000

typedef struct {
    const syscall_itest_case_t *spec;
    pcb_t *proc;
    uint32_t pid;
    uint8_t spawned;
    uint8_t finished;
    uint8_t passed;
    int32_t observed_exit_code;
} itest_state_t;

static int validate_blob(const syscall_itest_case_t *c) {
    if (!c->blob_start || !c->blob_end || c->blob_end <= c->blob_start) {
        KLOG_ERROR("SYSCALL_ITEST",
                   "case '%s' has an empty/unlinked blob (start=%p, end=%p).\n",
                   c->name, c->blob_start, c->blob_end);
        return 0;
    }
    return 1;
}

static void ensure_interrupts_enabled(void) {
    uint32_t eflags;
    __asm__ __volatile__("pushf; pop %0" : "=r"(eflags));
    if (!(eflags & 0x200)) {
        __asm__ __volatile__("sti");
    }
}

uint32_t syscall_itest_run_all(const syscall_itest_case_t *cases,
                               uint32_t count) {
    itest_state_t states[SYSCALL_ITEST_MAX];
    uint32_t remaining = 0;

    if (!cases || count == 0) {
        KLOG_INFO("SYSCALL_ITEST", "no syscall integration cases to run.\n");
        return 0;
    }

    if (count > SYSCALL_ITEST_MAX) {
        KLOG_ERROR("SYSCALL_ITEST", "too many cases (%u), capping at %u.\n",
                   count, SYSCALL_ITEST_MAX);
        count = SYSCALL_ITEST_MAX;
    }

    /* Phase 1: spawn every case as a user process. */
    for (uint32_t i = 0; i < count; i++) {
        itest_state_t *s = &states[i];
        s->spec = &cases[i];
        s->proc = NULL;
        s->pid = 0;
        s->spawned = 0;
        s->finished = 0;
        s->passed = 0;
        s->observed_exit_code = 0;

        if (!validate_blob(s->spec)) {
            continue; /* counted as a failure in the report phase */
        }

        uint32_t size = (uint32_t)(s->spec->blob_end - s->spec->blob_start);
        pcb_t *proc = userproc_create_from_blob(s->spec->name,
                                                s->spec->blob_start, size);
        if (!proc) {
            KLOG_ERROR("SYSCALL_ITEST",
                       "case '%s' failed to spawn user process.\n",
                       s->spec->name);
            continue;
        }

        s->proc = proc;
        s->pid = proc->pid;
        s->spawned = 1;
        remaining++;

        KLOG_VERBOSE("SYSCALL_ITEST",
                     "spawned case '%s' as pid=%u (expecting exit code %d).\n",
                     s->spec->name, s->pid, s->spec->expected_exit_code);
    }

    /* Phase 2: wait for the spawned processes to terminate, snapshotting each
     * exit code the first time we observe it terminated (before the idle
     * reaper can reclaim the PCB). */
    uint32_t start_tick = tick_count;
    while (remaining > 0 &&
           (tick_count - start_tick) < SYSCALL_ITEST_TIMEOUT_TICKS) {
        for (uint32_t i = 0; i < count; i++) {
            itest_state_t *s = &states[i];
            if (s->finished || !s->spawned) {
                continue;
            }

            pcb_t *p = s->proc;
            /* pid check guards against the PCB being reclaimed and reused. */
            if (p && p->pid == s->pid && p->has_exited) {
                s->observed_exit_code = p->exit_code;
                s->passed =
                    (p->exit_code == s->spec->expected_exit_code) ? 1 : 0;
                s->finished = 1;
                remaining--;
            }
        }

        ensure_interrupts_enabled();
        __asm__ __volatile__("hlt");
    }

    /* Phase 3: report a PASS/FAIL line per case and a summary. */
    uint32_t failed = 0;
    for (uint32_t i = 0; i < count; i++) {
        itest_state_t *s = &states[i];

        if (!s->spawned) {
            KLOG_ERROR("SYSCALL_ITEST", "[FAIL] %s: did not spawn.\n",
                       s->spec->name);
            failed++;
            continue;
        }

        if (!s->finished) {
            KLOG_ERROR("SYSCALL_ITEST",
                       "[FAIL] %s (pid=%u): timed out, never terminated.\n",
                       s->spec->name, s->pid);
            failed++;
            continue;
        }

        if (s->passed) {
            KLOG_INFO("SYSCALL_ITEST", "[PASS] %s (pid=%u): exit code %d.\n",
                      s->spec->name, s->pid, s->observed_exit_code);
        } else {
            KLOG_ERROR("SYSCALL_ITEST",
                       "[FAIL] %s (pid=%u): expected exit %d, got %d.\n",
                       s->spec->name, s->pid, s->spec->expected_exit_code,
                       s->observed_exit_code);
            failed++;
        }
    }

    if (failed == 0) {
        KLOG_INFO("SYSCALL_ITEST", "All %u syscall integration cases passed.\n",
                  count);
    } else {
        KLOG_ERROR("SYSCALL_ITEST",
                   "%u of %u syscall integration cases FAILED.\n", failed,
                   count);
    }

    return failed;
}
