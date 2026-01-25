#include "arch/x86/interrupt.h"
#include "printk.h"

void syscall_interrupt_handler(uint32_t idt_index, regs_t *regs) {
    printk("[SYSCALL] Syscall interrupt fired! eax=0x%08lx\n", regs->eax);
}
