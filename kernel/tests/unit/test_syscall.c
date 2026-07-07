#include "arch/x86/interrupt.h"
#include "lib/printk.h"
#include "lib/string.h"
#include "proc/syscall.h"
#include <stddef.h>
#include <stdint.h>

/*
 * Syscall unit tests.
 *
 * These tests exercise the *public* syscall surface only:
 *   - syscall_table_init()
 *   - syscall_table[]
 *   - syscall_interrupt_handler()
 *
 * They intentionally avoid any path that depends on `current_proc` or paging
 * so that they can run in plain kernel context with no process/VM setup.
 * Handlers that touch user memory (write/open/getpid) belong to the
 * integration suite where a real user process and page directory exist.
 */

/* Sentinel return value used to confirm the dispatcher forwards the handler's
 * return value back into regs->eax unchanged. */
#define SYSCALL_TEST_SENTINEL 0x5A5A1234

/* A free table slot (NUM_SYSCALLS == 8, slots 1..5 are used by real handlers).
 * Slot 6 is guaranteed NULL after syscall_table_init(). */
#define SYSCALL_TEST_FREE_SLOT 6

/* Records the arguments the probe handler was invoked with, so the test can
 * assert the register -> argument marshaling order. */
static uint32_t probe_args[6];
static uint8_t probe_called;

static int32_t syscall_marshal_probe(uint32_t a1, uint32_t a2, uint32_t a3,
                                     uint32_t a4, uint32_t a5, uint32_t a6) {
    probe_called = 1;
    probe_args[0] = a1;
    probe_args[1] = a2;
    probe_args[2] = a3;
    probe_args[3] = a4;
    probe_args[4] = a5;
    probe_args[5] = a6;
    return (int32_t)SYSCALL_TEST_SENTINEL;
}

/* Build a register snapshot as the syscall stub would present it:
 *   eax = syscall number
 *   ebx -> arg1, ecx -> arg2, edx -> arg3, esi -> arg4, edi -> arg5, ebp ->
 * arg6
 */
static void make_regs(regs_t *r, uint32_t num, uint32_t a1, uint32_t a2,
                      uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6) {
    memset(r, 0, sizeof(*r));
    r->eax = num;
    r->ebx = a1;
    r->ecx = a2;
    r->edx = a3;
    r->esi = a4;
    r->edi = a5;
    r->ebp = a6;
}

/* syscall_table_init_test - init populates known handlers and leaves the
 * reserved/unused slots NULL. */
uint8_t syscall_table_init_test(void) {
    syscall_table_init();

    if (!syscall_table[SYS_EXIT] || !syscall_table[SYS_WRITE] ||
        !syscall_table[SYS_GETPID] || !syscall_table[SYS_SCHED_YIELD] ||
        !syscall_table[SYS_OPEN] || !syscall_table[SYS_READ] ||
        !syscall_table[SYS_CLOSE] || !syscall_table[SYS_LSEEK]) {
        KLOG_ERROR("SYSCALL_TEST",
                   "table init test failed. one or more known handlers were "
                   "not registered.\n");
        return 0;
    }

    /* slot 0 and the reserved tail slots must stay NULL */
    if (syscall_table[0] != NULL ||
        syscall_table[SYSCALL_TEST_FREE_SLOT] != NULL) {
        KLOG_ERROR("SYSCALL_TEST",
                   "table init test failed. an unused slot was not NULL.\n");
        return 0;
    }

    KLOG_INFO("SYSCALL_TEST", "table init test passed.\n");
    return 1;
}

/* syscall_dispatch_unregistered_test - a valid index that has no handler
 * installed must yield ENOSYS without invoking anything. */
uint8_t syscall_dispatch_unregistered_test(void) {
    regs_t r;

    syscall_table_init();
    make_regs(&r, SYSCALL_TEST_FREE_SLOT, 0, 0, 0, 0, 0, 0);

    syscall_interrupt_handler(0x80, &r);

    if ((int32_t)r.eax != ENOSYS) {
        KLOG_ERROR("SYSCALL_TEST",
                   "unregistered dispatch test failed. expected ENOSYS (%d), "
                   "got %d.\n",
                   ENOSYS, (int32_t)r.eax);
        return 0;
    }

    KLOG_INFO("SYSCALL_TEST", "unregistered dispatch test passed.\n");
    return 1;
}

/* syscall_dispatch_out_of_range_test - an index >= NUM_SYSCALLS must yield
 * ENOSYS and never index past the table. */
uint8_t syscall_dispatch_out_of_range_test(void) {
    regs_t r;

    syscall_table_init();
    make_regs(&r, NUM_SYSCALLS + 100, 0, 0, 0, 0, 0, 0);

    syscall_interrupt_handler(0x80, &r);

    if ((int32_t)r.eax != ENOSYS) {
        KLOG_ERROR("SYSCALL_TEST",
                   "out-of-range dispatch test failed. expected ENOSYS (%d), "
                   "got %d.\n",
                   ENOSYS, (int32_t)r.eax);
        return 0;
    }

    KLOG_INFO("SYSCALL_TEST", "out-of-range dispatch test passed.\n");
    return 1;
}

/* syscall_dispatch_marshal_test - the dispatcher forwards ebx..ebp to the
 * handler arguments in the correct order and writes the handler's return value
 * back into eax. A throwaway probe handler is installed in a free slot and then
 * restored. */
uint8_t syscall_dispatch_marshal_test(void) {
    regs_t r;
    syscall_handler_t saved;

    syscall_table_init();

    saved = syscall_table[SYSCALL_TEST_FREE_SLOT];
    syscall_table[SYSCALL_TEST_FREE_SLOT] = syscall_marshal_probe;

    probe_called = 0;
    make_regs(&r, SYSCALL_TEST_FREE_SLOT, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66);

    syscall_interrupt_handler(0x80, &r);

    /* restore the table before asserting so a failure cannot leave a dangling
     * test handler installed. */
    syscall_table[SYSCALL_TEST_FREE_SLOT] = saved;

    if (!probe_called) {
        KLOG_ERROR("SYSCALL_TEST",
                   "marshal dispatch test failed. handler was not invoked.\n");
        return 0;
    }

    if (probe_args[0] != 0x11 || probe_args[1] != 0x22 ||
        probe_args[2] != 0x33 || probe_args[3] != 0x44 ||
        probe_args[4] != 0x55 || probe_args[5] != 0x66) {
        KLOG_ERROR("SYSCALL_TEST",
                   "marshal dispatch test failed. argument order mismatch: "
                   "a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x a5=0x%08x "
                   "a6=0x%08x.\n",
                   probe_args[0], probe_args[1], probe_args[2], probe_args[3],
                   probe_args[4], probe_args[5]);
        return 0;
    }

    if (r.eax != (uint32_t)SYSCALL_TEST_SENTINEL) {
        KLOG_ERROR("SYSCALL_TEST",
                   "marshal dispatch test failed. return value not propagated "
                   "to eax. expected 0x%08x, got 0x%08x.\n",
                   (uint32_t)SYSCALL_TEST_SENTINEL, r.eax);
        return 0;
    }

    KLOG_INFO("SYSCALL_TEST", "marshal dispatch test passed.\n");
    return 1;
}

/* syscall_write_bad_fd_test - SYS_WRITE rejects any descriptor other than
 * stdout (fd == 1) before touching user memory, returning -1. */
uint8_t syscall_write_bad_fd_test(void) {
    int32_t ret;

    syscall_table_init();

    /* fd = 2 is not stdout; handler must bail out early with -1. */
    ret = syscall_table[SYS_WRITE](2, 0, 0, 0, 0, 0);

    if (ret != -1) {
        KLOG_ERROR("SYSCALL_TEST",
                   "write bad-fd test failed. expected -1, got %d.\n", ret);
        return 0;
    }

    KLOG_INFO("SYSCALL_TEST", "write bad-fd test passed.\n");
    return 1;
}
