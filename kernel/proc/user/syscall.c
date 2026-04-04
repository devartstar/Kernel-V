#include "proc/syscall.h"
#include "lib/print_macros.h"
#include "lib/printk.h"
#include "proc/proc.h"

extern pcb_t *current_proc;

syscall_handler_t syscall_table[NUM_SYSCALLS] = {0};

static int32_t syscall_test(uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                            uint32_t e, uint32_t f) {
    KLOG_VERBOSE("SYSCALL",
                 "SYSCALL TEST METHOD with argument a=%u, b=%u, c=%u, d=%u, "
                 "e=%u, f=%u\n",
                 a, b, c, d, e, f);
    return 0xDEADC0DE;
}

static int32_t syscall_exit(uint32_t code, uint32_t _2, uint32_t _3,
                            uint32_t _4, uint32_t _5, uint32_t _6) {
    (void)_2;
    (void)_3;
    (void)_4;
    (void)_5;
    (void)_6;

    KLOG_INFO("SYSCALL", "syscall_exit called: code=%d, pid=%d\n", code,
              current_proc->pid);

    /* Mark the process as terminated and yield */
    current_proc->state = PROC_TERMINATED;
    dequeue_ready(current_proc);
    yield();

    /* should not reach here */
    while (1) {
    };
}

static int32_t syscall_write(uint32_t fd, uint32_t buf_ptr, uint32_t len,
                             uint32_t _4, uint32_t _5, uint32_t _6) {
    (void)_4;
    (void)_5;
    (void)_6;

    /* Only supports fd = 1 (stdout) */
    if (fd != 1)
        return -1;

    const char *buf = (const char *)buf_ptr;

    char kbuf[256];

    if (len > sizeof(kbuf) - 1)
        len = sizeof(kbuf) - 1;

    for (uint32_t i = 0; i < len; i++) {
        kbuf[i] = buf[i];
    }

    kbuf[len] = '\0';

    KLOG_INFO("SYSCALL",
              "sycall_write: buf=0x%08x (msg: %s) len=%u from pid=%d\n", buf,
              kbuf, len, current_proc->pid);

    return len;
}

static int32_t syscall_getpid(uint32_t _1, uint32_t _2, uint32_t _3,
                              uint32_t _4, uint32_t _5, uint32_t _6) {

    (void)_1;
    (void)_2;
    (void)_3;
    (void)_4;
    (void)_5;
    (void)_6;

    KLOG_INFO("SYSCALL", "syscall_getpid: %d\n", current_proc->pid);

    return current_proc->pid;
}

void syscall_table_init(void) {
    /* Register default handler (ENOSYS) for all syscalls */
    for (int8_t i = 0; i < NUM_SYSCALLS; i++) {
        syscall_table[i] = NULL;
    }

    syscall_table[SYS_EXIT] = syscall_exit;
    syscall_table[SYS_WRITE] = syscall_write;
    syscall_table[SYS_GETPID] = syscall_getpid;
}

void syscall_interrupt_handler(uint32_t idt_index, regs_t *regs) {
    (void)idt_index;

    uint32_t num = regs->eax;
    uint32_t arg1 = regs->ebx;
    uint32_t arg2 = regs->ecx;
    uint32_t arg3 = regs->edx;
    uint32_t arg4 = regs->esi;
    uint32_t arg5 = regs->edi;
    uint32_t arg6 = regs->ebp;

    KLOG_VERBOSE("SYSCALL", "Syscall interrupt fired! Number=0x%08lx\n",
                 PRINT_UINT32(num));

    // For now, just handle it as a no-op and return
    // The interrupt will return to user mode automatically

    int32_t retval = ENOSYS;

    if (num < NUM_SYSCALLS && syscall_table[num]) {
        KLOG_VERBOSE("SYSCALL", "Invoking syscall handler at address 0x%08x\n",
                     syscall_table[num]);
        retval = syscall_table[num](arg1, arg2, arg3, arg4, arg5, arg6);
    } else {
        KLOG_VERBOSE("SYSCALL", "No syscall handler for Number=0x%08x\n", num);
    }

    KLOG_VERBOSE("SYSCALL", "Syscall handler returned value = 0x%08x\n",
                 retval);

    /* Kernel syscall handler on return value is stored in eax register */
    regs->eax = retval;
}
