#include "arch/x86/usermode_stub.h"
#include <stdint.h>

// These selectors must match your GDT layout
#define USER_CS 0x1B // User code segment selector (index 3, RPL=3)
#define USER_DS 0x23 // User data segment selector (index 4, RPL=3)

void switch_to_usermode(uint32_t entry, uint32_t user_stack_top) {
    __asm__ __volatile__(
        "cli\n\t"
        // Set data segments to user data
        "mov $0x23, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        // Push SS, ESP, EFLAGS, CS, EIP for IRET
        "pushl $0x23\n\t" // SS (user data segment)
        "pushl %0\n\t"    // ESP (top of user stack)
        "pushf\n\t"       // EFLAGS
        "popl %%eax\n\t"
        "orl $0x200, %%eax\n\t" // Ensure IF (interrupt flag) is set
        "pushl %%eax\n\t"
        "pushl $0x1B\n\t" // CS (user code segment)
        "pushl %1\n\t"    // EIP (entry point)
        "iret\n\t"
        :
        : "r"(user_stack_top), "r"(entry)
        : "ax", "memory");
    __builtin_unreachable();
}
