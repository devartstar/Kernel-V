#include "arch/x86/usermode_stub.h"
#include "lib/printk.h"
#include "lib/string.h"
#include "mm/paging.h"
#include "proc/user.h"
#include <string.h>

void test_usermode_process(void) {
    /* Allocate stack for the process */

    KLOG_VERBOSE("TEST", "Inside Usermode Process Test\n");

    int stack_allocatable =
        map_high_stack(USER_STACK_BOTTOM_VIRT, USER_STACK_TOP_VIRT,
                       PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    if (stack_allocatable == -1) {
        KLOG_VERBOSE("TEST", "Failed to allocate user stack.\n");
        return;
    }

    memset((void *)USER_STACK_BOTTOM_VIRT, 0xCC, USER_SPACE_STACK_SIZE);

    uint32_t user_stack_top = USER_STACK_TOP_VIRT;

    KLOG_VERBOSE("TEST",
                 "Launching user mode test stub at 0x%08x with stack 0x%08x\n",
                 (uint32_t)&usermode_stub, USER_STACK_TOP_VIRT);

    switch_to_usermode((uint32_t)&usermode_stub, USER_STACK_TOP_VIRT);
}
