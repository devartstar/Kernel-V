#include "tests/syscall_itest.h"
#include "arch/x86/interrupt.h"
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

/* Wall-clock budget for the concurrent suite after spawning. Verbose serial
 * logging and explicit yields can make a successful run exceed ten seconds.
 * Keep a finite bound, expressed in terms of the configured PIT frequency. */
#define SYSCALL_ITEST_TIMEOUT_TICKS (30u * PIT_DEFAULT_HZ)

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

        /* Hold off the idle reaper until we have observed this process's exit
         * code. Without this, a process can terminate and be reclaimed (its
         * PCB freed back to the pool, which overwrites pid) before the harness
         * thread next polls it, making it look like it never terminated. */
        proc->reap_blocked = 1;

        KLOG_VERBOSE("SYSCALL_ITEST",
                     "spawned case '%s' as pid=%u (expecting exit code %d).\n",
                     s->spec->name, s->pid, s->spec->expected_exit_code);
    }

    /* Phase 2: wait for the spawned processes to terminate, snapshotting each
     * exit code the first time we observe it terminated (before the idle
     * reaper can reclaim the PCB). */
    uint32_t start_tick = tick_count;
    uint32_t waited_ticks;
    for (;;) {
        /* On this single-CPU kernel, freeze exit/reaper state while taking
         * the snapshot. Always inspect completion BEFORE the deadline check,
         * including the first wakeup at or after the deadline. */
        irq_flags_t flags = irq_save();
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

                /* We have snapshotted everything we need; let the idle reaper
                 * reclaim this PCB now. */
                p->reap_blocked = 0;
            }
        }

        waited_ticks = tick_count - start_tick;
        if (remaining == 0 || waited_ticks >= SYSCALL_ITEST_TIMEOUT_TICKS) {
            /* Freeze the verdicts before any reporting can be preempted.
             * Release remaining reap holds now; never dereference these PCBs
             * again, since a late exit can reclaim them while we print. */
            for (uint32_t i = 0; i < count; i++) {
                itest_state_t *s = &states[i];
                if (s->spawned && !s->finished && s->proc &&
                    s->proc->pid == s->pid) {
                    s->proc->reap_blocked = 0;
                }
            }
            irq_restore(flags);
            break;
        }

        /* Enable IRQs and sleep atomically with respect to IRQ delivery.
         * No logging or scheduling with IRQs disabled in the snapshot above. */
        __asm__ __volatile__("sti; hlt" ::: "memory");
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
                       "[FAIL] %s (pid=%u): completion not observed before "
                       "wait ended (waited=%u ticks, budget=%u ticks).\n",
                       s->spec->name, s->pid, waited_ticks,
                       SYSCALL_ITEST_TIMEOUT_TICKS);
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
