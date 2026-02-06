#include "proc/syscall.h"
#include "lib/print_macros.h"
#include "lib/printk.h"

syscall_handler_t syscall_table[NUM_SYSCALLS] = {0};

static int32_t syscall_test(uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                            uint32_t e, uint32_t f) {
    KLOG_VERBOSE("SYSCALL",
                 "SYSCALL TEST METHOD with argument a=%u, b=%u, c=%u, d=%u, "
                 "e=%u, f=%u\n",
                 a, b, c, d, e, f);
    return 0xDEADC0DE;
}

void syscall_table_init(void) {
    /* Register default handler (ENOSYS) for all syscalls */
    for (int8_t i = 0; i < NUM_SYSCALLS; i++) {
        syscall_table[i] = NULL;
    }

    syscall_table[0] = syscall_test;
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
        KLOG_VERBOSE("SYSCALL", "Invoking syscall handler at address 0x%08x",
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
