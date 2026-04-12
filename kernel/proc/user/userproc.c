#include "arch/x86/usermode_stub.h"
#include "core/panik.h"
#include "lib/printk.h"
#include "lib/string.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "proc/proc.h"
#include "proc/user.h"
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

static void user_map_region(uint32_t virt_start, uint32_t size,
                            uint32_t flags) {
    /* start page and end page for the given virt address
     *-----|s|--<v_s>---------<v_e>-|e|--- */
    uint32_t start = PAGE_ALIGN_DOWN(virt_start);
    uint32_t end = PAGE_ALIGN_UP(virt_start + size);

    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
        void *phys = pmm_alloc_frame();
        if (!phys) {
            panik("user_map_region: pmm_alloc_frame failed");
        }

        paging_map_page(addr, (uint32_t)phys, flags);
    }
}

static void user_zero_region(uint32_t virt_start, uint32_t size) {
    memset((void *)virt_start, 0, size);
}

void userproc_kernel_entry(void *arg) {
    (void)arg;

    pcb_t *proc = current_proc;

    if (!proc) {
        panik("userproc_kernel_entry: proc is NULL");
    }

    if (proc != current_proc) {
        panik("userproc_kernel_entry: Proc doesn't match Current runninf "
              "process");
    }

    if (proc->type != PROC_TYPE_USER) {
        panik("userproc_kernel_entry: current process is not USER");
    }

    if (!proc->user_entry || !proc->user_stack_top) {
        panik("userproc_kernel_entry: invalid user entry/stack");
    }

    KLOG_INFO(
        "USERPROC",
        "Entering usermode: name=%d (pid=%u) entry=0x%08x stack_top=0x%08x\n",
        proc->name, proc->pid, proc->user_entry, proc->kernel_stack_top);

    /* Switch to Usermode */
    switch_to_usermode(proc->user_entry, proc->user_stack_top - 4);

    panik("userproc_kernel_entry: returned from switch_to_usermode");
}

pcb_t *userproc_create_from_blob(const char *name, const uint8_t *blob_start,
                                 uint32_t blob_size) {
    pcb_t *proc;

    if (!name || !blob_start || blob_size == 0) {
        KLOG_ERROR("USERPROC", "Invalid arguments: name=%s, blob=%p, size=%u\n",
                   name, blob_start, blob_size);
        return NULL;
    }

    /* Create a schedulable kernel-mode processes whose execution starts from
     * userproc_kenrel_entry */
    proc = proc_create(userproc_kernel_entry, NULL, name);
    if (!proc) {
        KLOG_ERROR("USERPROC", "Process creation failed\n");
        return NULL;
    }

    /*
     * Prevent the scheduler from picking this process before we finish
     * setting up user metadata.  proc_create() already enqueued it as
     * PROC_READY; temporarily mark it PROC_NEW so scheduler_pick_next()
     * skips it.
     */
    proc->state = PROC_NEW;

    proc_set_type(proc, PROC_TYPE_USER);

    proc->parent = current_proc;

    /* Userspace metadata */
    proc->user_entry = USER_CODE_VIRT;
    proc->user_code_size = blob_size;
    proc->user_stack_top = USER_STACK_TOP_VIRT;
    proc->user_stack_size = USER_STACK_SIZE;

    /* Map user code pages */
    user_map_region(proc->user_entry, proc->user_code_size,
                    PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    /* Map user stack pages */
    user_map_region(proc->user_stack_top - proc->user_stack_size,
                    proc->user_stack_size,
                    PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    /* Initialize memory contents */
    memcpy((void *)proc->user_entry, blob_start, blob_size);
    user_zero_region(proc->user_stack_top - proc->user_stack_size,
                     proc->user_stack_size);

    /* Setup complete — allow scheduling */
    proc->state = PROC_READY;

    KLOG_INFO("USERPROC",
              "Created user process: name=%s (pid=%u, type=%s), entry=0x%08x "
              "code_size=0x%08x, stack_top=0x%08x, stack_size=0x%08x\n",
              proc->name, proc->pid, proc_type_to_string(proc->type),
              proc->user_entry, proc->user_code_size, proc->user_stack_top,
              proc->user_stack_size);

    return proc;
}
