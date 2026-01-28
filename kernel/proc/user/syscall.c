#include "arch/x86/interrupt.h"
#include "lib/printk.h"

void syscall_interrupt_handler(uint32_t idt_index, regs_t *regs) {
    KLOG_VERBOSE("SYSCALL", "Syscall interrupt fired! eax=0x%08lx\n", regs->eax);
    
    // For now, just handle it as a no-op and return
    // The interrupt will return to user mode automatically
    
    // You could add syscall number handling here based on regs->eax
    switch (regs->eax) {
        case 0x12345678:
            KLOG_VERBOSE("SYSCALL", "Test syscall received - success!\n");
            regs->eax = 0; // Return success
            break;
        default:
            KLOG_VERBOSE("SYSCALL", "Unknown syscall: 0x%08lx\n", regs->eax);
            regs->eax = -1; // Return error
            break;
    }
}
