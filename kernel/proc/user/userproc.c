#include "arch/x86/usermode_stub.h"
#include "core/panik.h"
#include "lib/printk.h"
#include "proc/proc.h"
#include <stdint.h>

// These selectors must match your GDT layout
#define USER_CS 0x1B // User code segment selector (index 3, RPL=3)
#define USER_DS 0x23 // User data segment selector (index 4, RPL=3)

void switch_to_usermode(uint32_t entry, uint32_t user_stack_top) {

    if (!current_proc || current_proc->type != PROC_TYPE_USER) {
        panik("Trying to switch to user mode from a non-user process\n");
    }

    KLOG_VERBOSE("PRIVILEGE", "Switching to user mode privilege for test\n");

    // Simpler approach - don't change segments before iret
    __asm__ __volatile__(
        "cli\n\t"

        // Push iret frame for privilege level change (SS, ESP, EFLAGS, CS, EIP)
        "pushl $0x23\n\t"  // SS (user data segment)
        "pushl %0\n\t"     // ESP (user stack)
        "pushl $0x202\n\t" // EFLAGS (IF=1, bit 1 reserved=1)
        "pushl $0x1B\n\t"  // CS (user code segment)
        "pushl %1\n\t"     // EIP (entry point)

        "iret\n\t" // Switch to user mode
        :
        : "r"(user_stack_top), "r"(entry)
        : "memory");

    KLOG_VERBOSE("PRIVILEGE", "Returned back from the user mode after test\n");
}
