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

static void user_map_region_in_pd(uint32_t *pd_virt, uint32_t virt_start,
                                  uint32_t size, uint32_t flags) {
    /* start page and end page for the given virt address
     *-----|s|--<v_s>---------<v_e>-|e|--- */
    uint32_t start = virt_start & 0xFFFFF000;
    uint32_t end = (virt_start + size + 0xFFF) & 0xFFFFF000;

    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
        void *phys = pmm_alloc_frame();
        if (!phys) {
            panik("user_map_region: pmm_alloc_frame failed");
        }

        paging_map_page_in_pd(pd_virt, addr, (uint32_t)phys, flags);
    }
}

void userproc_kernel_entry(void *args) {
    (void)args;
    if (!current_proc || current_proc->type != PROC_TYPE_USER) {
        panik("userproc_kernel_entry: not a user process\n");
    }
    switch_to_usermode(current_proc->user_entry, current_proc->user_stack_top);
}

pcb_t *userproc_alloc(const char *name) {
    pcb_t *proc = proc_create(userproc_kernel_entry, NULL, name);
    if (!proc) {
        return NULL;
    }

    proc_set_type(proc, PROC_TYPE_USER);

    if (paging_create_address_space(&proc->page_directory_virt,
                                    &proc->page_directory_phys) != 0) {
        return NULL;
    }

    proc->user_entry = 0;
    proc->user_code_size = 0;
    proc->user_stack_top = 0;
    proc->user_stack_size = 0;

    return proc;
}

int userproc_load_blob(pcb_t *proc, const uint8_t *blob_start,
                       uint32_t blob_size) {
    uint32_t code_start = USER_CODE_VIRT;
    uint32_t stack_top = USER_STACK_TOP_VIRT;
    uint32_t stack_size = USER_STACK_SIZE;
    uint32_t stack_bottom = stack_top - stack_size;

    if (!proc || !blob_start || blob_size == 0) {
        return -1;
    }

    if (proc->type != PROC_TYPE_USER) {
        return -1;
    }

    if (!proc->page_directory_virt || !proc->page_directory_phys) {
        return -1;
    }

    user_map_region_in_pd(proc->page_directory_virt, code_start, blob_size,
                          PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    user_map_region_in_pd(proc->page_directory_virt, stack_bottom, stack_size,
                          PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    uint32_t old_cr3 = paging_get_current_cr3();
    paging_switch_address_space(proc->page_directory_phys);

    memcpy((void *)code_start, blob_start, blob_size);
    memset((void *)stack_bottom, 0, stack_size);

    paging_switch_address_space(old_cr3);

    proc->user_entry = code_start;
    proc->user_code_size = blob_size;
    proc->user_stack_top = stack_top;
    proc->user_stack_size = stack_size;

    return 0;
}

pcb_t *userproc_create_from_blob(const char *name, const uint8_t *blob_start,
                                 uint32_t blob_size) {
    pcb_t *proc = userproc_alloc(name);
    if (!proc) {
        return NULL;
    }

    if (userproc_load_blob(proc, blob_start, blob_size) != 0) {
        return NULL;
    }

    /* Process is fully set up — now make it schedulable */
    print_proc_info(proc);
    proc_mark_ready(proc);

    return proc;
}
