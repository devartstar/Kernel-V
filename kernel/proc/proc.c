#include "proc/proc.h"
#include "arch/x86/interrupt.h"
#include "arch/x86/tss.h"
#include "core/debug.h"
#include "core/panik.h"
#include "fs/fd.h"
#include "lib/printk.h"
#include "lib/string.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "mm/pool_alloc.h"
#include "proc/context_switch.h"
#include "proc/scheduler.h"
#include <stddef.h>

//  PID starts from 1
static uint32_t next_pid = 1;
pcb_t *current_proc = NULL;

pcb_t *proc_list_head = NULL;
pcb_t *proc_list_tail = NULL;

void cleanup_terminated_processes(void) {
    pcb_t *p = proc_list_head;

    while (p) {
        pcb_t *next = p->all_next;

        if (proc_is_reclaimable(p)) {
            debug_module(PROCESS_MGMT,
                         "Reclaiming process: %s (pid=%u, type=%s)\n", p->name,
                         p->pid, proc_type_to_string(p->type));
            dequeue_proc_list(p);
            proc_free(p);
        }

        p = next;
    }
}

static void idle_process(void *arg) {
    (void)arg;
    static uint8_t idle_count = 0;

    while (1) {
        idle_count++;

        /* Every 10 idle process - clean up terminated process */
        if (idle_count % 10 == 0) {
            KLOG_INFO("PRCOESS_MGMT", "[CLEANUP] Terminated Process\n");
            cleanup_terminated_processes();
        }

        yield();
    }
}

/**************************************
 * START: POOL ALLOCATOR for PCB      *
 * ************************************/

static pool_allocator_t pcb_pool;

void pcb_allocator_init() { pool_init(&pcb_pool, sizeof(pcb_t)); }

pcb_t *pcb_alloc() { return (pcb_t *)pool_alloc(&pcb_pool); }

void pcb_free(pcb_t *pcb) { pool_free(&pcb_pool, pcb); }

/****************************************
 * START: PROCESS MANAGEMENT            *
 * **************************************/

void proc_init(void) {
    proc_list_head = NULL;
    ready_list_head = NULL;
    wait_list_head = NULL;

    next_pid = 1;
    pcb_allocator_init();

    /* Create and IDLE process */
    pcb_t *idle = proc_create(idle_process, NULL, "idle");
    if (!idle) {
        panik("Failed creating IDLE process");
    }
    proc_set_type(idle, PROC_TYPE_IDLE);

    print_proc_info(idle);
    proc_mark_ready(idle);

    debug_module(PROCESS_MGMT, "Created idle process with PID %d\n", idle->pid);
}

pcb_t *proc_alloc(const char *name) {
    pcb_t *new_proc = pcb_alloc();
    if (!new_proc) {
        return NULL;
    }

    memset(new_proc, 0, sizeof(pcb_t));

    new_proc->pid = next_pid++;
    new_proc->state = PROC_NEW;
    new_proc->type = PROC_TYPE_UNASSIGNED;
    new_proc->exit_code = 0;
    new_proc->has_exited = 0;

    /* Background trace id for this process's non-syscall execution. Minted
     * without disturbing the caller's active sc. */
    new_proc->trace_id = log_trace_next();
    new_proc->trace_id_saved = new_proc->trace_id;

    new_proc->kernel_stack_base = NULL;
    new_proc->kernel_stack_top = NULL;
    new_proc->kernel_stack_size = 0;

    strncpy(new_proc->name, name, PROC_NAME_MAX);
    new_proc->name[PROC_NAME_MAX - 1] = '\0';

    new_proc->user_entry = 0;
    new_proc->user_code_size = 0;
    new_proc->user_stack_size = 0;
    new_proc->user_stack_top = 0;

    new_proc->parent = NULL;

    new_proc->all_next = NULL;
    new_proc->all_prev = NULL;

    new_proc->next = NULL;
    new_proc->prev = NULL;

    new_proc->page_directory_virt = kernel_page_directory_virt;
    new_proc->page_directory_phys = kernel_page_directory_phys;

    /* Track in the global process list (not yet schedulable) */
    enqueue_proc_list(new_proc);

    return new_proc;
}

void enqueue_proc_list(pcb_t *proc) {
    if (!proc) {
        return;
    }

    proc->all_next = NULL;
    proc->all_prev = NULL;

    if (proc_list_head) {
        /* Entry is already present */
        proc_list_tail->all_next = proc;
        proc->all_prev = proc_list_tail;
        proc_list_tail = proc;
    } else {
        /* First entry in proc_list */
        proc_list_head = proc;
        proc_list_tail = proc;
    }
}

void dequeue_proc_list(pcb_t *proc) {
    /* Guard against double-dequeue */
    if (!proc->all_prev && !proc->all_next && proc != proc_list_head) {
        return;
    }

    if (proc->all_prev) {
        /* If not the first process in list */
        proc->all_prev->all_next = proc->all_next;
    } else {
        /* First entry in the list */
        proc_list_head = proc->all_next;
    }

    if (proc->all_next) {
        /* If not the last process in list */
        proc->all_next->all_prev = proc->all_prev;
    } else {
        /* Last entry in the list */
        proc_list_tail = proc->all_prev;
    }

    proc->all_next = NULL;
    proc->all_prev = NULL;
}

void proc_cleanup_kernel(pcb_t *proc) {
    if (!proc) {
        return;
    }

    KLOG_VERBOSE("PROCESS_MGMT",
                 "Cleaning up kernel process: name=%s (pid=%u, type=%s) "
                 "kernel_stack_base=0x%08x, kernel_stack_top=0x%08x, "
                 "kernel_stack_size=0x%08x\n",
                 proc->name, proc->pid, proc_type_to_string(proc->type),
                 proc->kernel_stack_base, proc->kernel_stack_top,
                 proc->kernel_stack_size);

    // Don't set state here - should already be TERMINATED
    // Don't dequeue here - should already be dequeued

    /* Free up the process kernel stack memory */
    if (proc->kernel_stack_base && proc->kernel_stack_size) {
        for (uint32_t offset = 0; offset < proc->kernel_stack_size;
             offset += PAGE_SIZE) {
            pmm_free_frame((phys_addr_t)proc->kernel_stack_base + offset);
        }
    }

    proc->kernel_stack_base = NULL;
    proc->kernel_stack_top = NULL;
    proc->kernel_stack_size = 0;

    /* close all the file ref. for the process */
    fd_close_all(proc);

    /* Free PCB */
    pcb_free(proc);
}

void proc_cleanup_user(pcb_t *proc) {
    if (!proc) {
        return;
    }

    KLOG_VERBOSE(
        "PROCESS_MGMT",
        "Cleaning up user process: name=%s (pid=%u, type=%s) "
        "user_entry=0x%08x, user_stack_top=0x%08x, user_code_size=0x%08x\n",
        proc->name, proc->pid, proc_type_to_string(proc->type),
        proc->user_entry, proc->user_stack_top, proc->user_code_size)

    /* Free user code backing frame */
    if (proc->user_entry && (proc->user_code_size > 0)) {
        paging_free_region_in_pd(proc->page_directory_virt, proc->user_entry,
                                 proc->user_code_size);
    }

    /* Free user stack frames */
    if (proc->user_stack_top && proc->user_stack_size > 0) {
        uint32_t stack_bottom = proc->user_stack_top - proc->user_stack_size;
        paging_free_region_in_pd(proc->page_directory_virt, stack_bottom,
                                 proc->user_stack_size);
    }

    /* No need to free lower kernel half of PDE */
    /* Free the higher half PDE, skip PDE[0] (kernel identity map) */
    for (uint32_t i = 1; i < KERNEL_PDE_START; i++) {
        /* Get the physical address of the page directory entry */
        uint32_t pde = proc->page_directory_virt[i];
        if (pde & PAGE_PRESENT) {
            phys_addr_t pt_phys = (phys_addr_t)(pde & 0xFFFFF000);
            pmm_free_frame(pt_phys);
            proc->page_directory_virt[i] = 0;
        }
    }

    /* Free the Page Dir frame */
    pmm_free_frame(proc->page_directory_phys);
    proc->page_directory_virt = NULL;
    proc->page_directory_phys = 0;

    /* Free kernel stack */
    if (proc->kernel_stack_base && proc->kernel_stack_size) {
        uint32_t pages = proc->kernel_stack_size / PAGE_SIZE;
        uint8_t page = (phys_addr_t)proc->kernel_stack_base;

        for (uint32_t i = 0; i < pages; i++) {
            pmm_free_frame(page + i * PAGE_SIZE);
        }
    }

    proc->user_entry = 0;
    proc->user_code_size = 0;
    proc->user_stack_top = 0;
    proc->user_stack_size = 0;

    proc->kernel_stack_base = NULL;
    proc->kernel_stack_top = NULL;
    proc->kernel_stack_size = 0;

    /* close all the file ref. by the process */
    fd_close_all(proc);

    pcb_free(proc);
}

void proc_free(pcb_t *proc) {
    if (!proc) {
        return;
    }

    if (proc_is_special(proc)) {
        KLOG_ERROR(
            "PROCESS_MGMT",
            "Refused to free special process: name=%s (pid=%u, type=%s)\n",
            proc->name, proc->pid, proc_type_to_string(proc->type));

        return;
    }

    switch (proc->type) {
    case PROC_TYPE_KERNEL:
        proc_cleanup_kernel(proc);
        break;
    case PROC_TYPE_USER:
        proc_cleanup_user(proc);
        break;
    case PROC_TYPE_BOOTSTRAP:
    case PROC_TYPE_IDLE:
    default:
        KLOG_ERROR("PROCESS_MGMT",
                   "proc_free reached invalid/special process: pid=%u name=%s "
                   "type=%s\n",
                   proc->pid, proc->name, proc_type_to_string(proc->type));
    }

    return;
}

pcb_t *proc_find(uint32_t pid) {
    for (pcb_t *p = ready_list_head; p; p = p->next) {
        if (p->pid == pid) {
            return p;
        }
    }
    return NULL;
}

pcb_t *proc_create(void (*entry)(void *), void *arg, const char *name) {
    /* create a pcb for the process and fill it */
    pcb_t *proc = proc_alloc(name);
    if (!proc) {
        /* could not allocate memory to pcb */
        return NULL;
    }

    /* allocate a memory page as kernel stack to process */
    phys_addr_t kernel_stack_block = pmm_alloc_frame();
    if (!kernel_stack_block) {
        proc_free(proc);
        return NULL;
    }

    /* Identity map between kernel physical and virtual address space */
    proc->kernel_stack_base =
        (uint8_t *)phys_to_virt_identity(kernel_stack_block);
    proc->kernel_stack_size = KERNEL_STACK_SIZE;
    proc->kernel_stack_top = proc->kernel_stack_base + proc->kernel_stack_size;

    /* since stack grows downwards, stack pointer pointing to top of stack */
    virt_addr_t *stack_top = (virt_addr_t *)proc->kernel_stack_top;

    /*
     Update the stack to call the thread_entry_wrapper (entry, arg)
     When proc create it called instead of executing from the entry function
     It will start execution from thread_entry_wrapper which
     which takes entry func and args as its parameter.
    */

    /* push a fake return address (will never be used) */
    *(--stack_top) = 0;
    /* push the entry function to be used by thread_entry_wrapper */
    *(--stack_top) = (virt_addr_t)entry;
    /* push the argument pointer */
    *(--stack_top) = (virt_addr_t)arg;

    proc->context.esp = (virt_addr_t)(uintptr_t)stack_top;
    proc->context.eip = (virt_addr_t)(uintptr_t)thread_entry_wrapper;
    proc->context.ebp = 0;

    // Initialize EFLAGS with interrupts enabled
    proc->context.eflags =
        0x202; // IF (Interrupt Enable) bit set + reserved bit 1

    proc->has_exited = 0;
    proc->exit_code = 0;

    return proc;
}

void proc_set_type(pcb_t *proc, proc_type_t type) {
    if (!proc) {
        return;
    }

    proc->type = type;
}

void proc_mark_ready(pcb_t *proc) {
    if (!proc) {
        return;
    }

    proc->state = PROC_READY;
    enqueue_ready(proc);
}

const char *proc_type_to_string(proc_type_t type) {
    switch (type) {
    case PROC_TYPE_UNASSIGNED:
        return "UNASSIGNED";
    case PROC_TYPE_BOOTSTRAP:
        return "BOOTSTRAP";
    case PROC_TYPE_IDLE:
        return "IDLE";
    case PROC_TYPE_KERNEL:
        return "KERNEL";
    case PROC_TYPE_USER:
        return "USER";
    default:
        return "UNKNOWN";
    }
}

void proc_sleep(uint32_t ticks) {
    current_proc->state = PROC_WAITING;
    current_proc->sleep_ticks = ticks;

    dequeue_ready(current_proc);
    enqueue_wait(current_proc);

    /* Current process is put to sleep, schedule a new process to run */
    yield();
}

void proc_wakeup(pcb_t *proc) {
    dequeue_wait(proc);

    proc->state = PROC_READY;
    proc->sleep_ticks = 0;

    enqueue_ready(proc);
}

void proc_exit(void) {
    pcb_t *proc_now = current_proc;

    KLOG_VERBOSE("PROCESS_MGMT", "Process exiting: Name=%s (pid=%u, type=%s)\n",
                 proc_now->name, proc_now->pid,
                 proc_type_to_string(proc_now->type));

    /* Mark the process as terminated */
    proc_mark_terminated(proc_now, 0x0);
    dequeue_ready(proc_now);
    yield();

    /* Should never reach here - the cleanup will happen later
       when the idle process calls cleanup_terminated_processes() */
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

int proc_is_special(const pcb_t *proc) {
    if (!proc) {
        return 0;
    }

    return (proc->type == PROC_TYPE_BOOTSTRAP || proc->type == PROC_TYPE_IDLE);
}

int proc_is_reclaimable(const pcb_t *proc) {
    if (!proc || !proc->has_exited || proc->state != PROC_TERMINATED) {
        return 0;
    }

    return !proc_is_special(proc);
}

uint8_t proc_is_runnable(const pcb_t *proc) {
    /* Process should not be terminated */
    if (proc->state == PROC_TERMINATED) {
        return 0;
    }

    /* Process should be ready and not already running */
    if (proc->state != PROC_READY && proc->state != PROC_RUNNING) {
        return 0;
    }

    return 1;
}

void proc_mark_terminated(pcb_t *proc, int32_t exit_code) {
    if (!proc) {
        return;
    }

    if (proc_is_special(proc)) {
        KLOG_ERROR("PROCESS_MGMT",
                   "Refusing to terminate special process: name=%s (pid=%u, "
                   "type=%s)\n",
                   proc->name, proc->pid, proc_type_to_string(proc->type));
        return;
    }

    proc->exit_code = exit_code;
    proc->has_exited = 1;
    proc->state = PROC_TERMINATED;

    KLOG_INFO("PROCESS_MGMT",
              "Process is marked as terminated: name=%s (pid=%u, type=%s) with "
              "exit code=0x%08x\n",
              proc->name, proc->pid, proc_type_to_string(proc->type),
              proc->exit_code);
}

void proc_bootstrap_handoff(void) {
    irq_flags_t flags;

    if (!current_proc) {
        panik("proc_bootstrap_handoff: current_proc is NULL");
    }

    if (current_proc->type != PROC_TYPE_BOOTSTRAP) {
        panik("proc_bootstrap_handoff: current process is not BOOTSTRAP");
    }

    KLOG_INFO("PROCESS_MGMT",
              "Bootstrap handoff: name=%s (pid=%u, type=%s) removed from "
              "normal scheduling.\n",
              current_proc->name, current_proc->pid,
              proc_type_to_string(current_proc->type));

    flags = irq_save();

    dequeue_ready(current_proc);
    current_proc->state = PROC_WAITING;

    irq_restore(flags);

    return;
}

/*****************************************
 * START: PROCESS SCHEDULING             *
 * ***************************************/

pcb_t *scheduler_pick_next(void) {
    pcb_t *proc_now = current_proc;
    pcb_t *proc_next = NULL;

    if (!current_proc) {
        return ready_list_head;
    }

    /* Round-robin scheduling: iterate through all processes starting from next
     */
    pcb_t *start_proc = proc_now->next ? proc_now->next : ready_list_head;
    proc_next = start_proc;

    /* Look for a READY process (skip SLEEPING and TERMINATED processes) */
    do {
        if (proc_next && proc_is_runnable(proc_next)) {
            return proc_next;
        }

        proc_next = proc_next ? proc_next->next : ready_list_head;

        if (proc_next == start_proc) {
            break;
        }
    } while (proc_next != proc_now);

    if (proc_now && proc_now->state == PROC_TERMINATED) {
        /*  If no process ready to run, find and return idle process */
        for (pcb_t *p = ready_list_head; p; p = p->next) {
            if (strcmp(p->name, "idle") == 0) {
                return p;
            }
        }
    }

    /* If no READY process found, return the idle process */
    for (pcb_t *p = ready_list_head; p; p = p->next) {
        if (strcmp(p->name, "idle") == 0) {
            return p;
        }
    }

    return proc_now;
}

/*
 * yield is called form multiple places, can be categorized into 2:
 * Manual - idle after cleanup, sleep, kerne main loop monitoring, tests
 * TIMER - when the process is out of timeslice
 */
void yield(void) {
    pcb_t *proc_now = current_proc;
    pcb_t *proc_next = NULL;

    /* If current process is invalid */
    if (!proc_now || proc_now->pid <= 0) {
        pr_error("CURRENT PROCESS CORRUPTED: pid=%d, name=%s\n",
                 proc_now ? (int)proc_now->pid : -1,
                 proc_now ? proc_now->name : "NULL");
        panik("process corrupted");
    }

    proc_next = scheduler_pick_next();

    /* Debug the new process contexts */
    if (proc_next) {
        KLOG_VERBOSE("PROCESS_MGMT",
                     "Switching to %s: EIP=0x%08x ESP=0x%08x EFLAGS=0x%08x\n",
                     proc_next->name, PRINT_UINT32(proc_next->context.eip),
                     PRINT_UINT32(proc_next->context.esp),
                     PRINT_UINT32(proc_next->context.eflags));

        /* Todo: Check condition if interrupts disabled and compare with
         * proc_next->context.eflag */
    }

    /* Set timeslice for all processes, including idle (but give idle only 1
     * tick) */
    if (proc_next) {
        if (strcmp(proc_next->name, "idle") == 0) {
            proc_next->timeslice_ticks = 1;
        } else {
            proc_next->timeslice_ticks = DEFAULT_TIMESLICE;
        }
    }

    if (proc_next && proc_next != proc_now) {

        KLOG_VERBOSE("PROCESS_MGMT",
                     "New current process: %s (pid=%u, type=%s)\n",
                     proc_next->name, proc_next->pid,
                     proc_type_to_string(proc_next->type));

        KLOG_VERBOSE(
            "PROCESS_MGMT",
            "About to switch: \n\t"
            "Prev Process=%s (PID=%u, Type=%s) (eflags=0x%lx) \n\t"
            "New Process=%s  (PID=%u, Type=%s) (eflags=0x%lx) \n",
            proc_now->name, proc_now->pid, proc_type_to_string(proc_now->type),
            proc_now->context.eflags, proc_next->name, proc_next->pid,
            proc_type_to_string(proc_next->type), proc_next->context.eflags);

        /*
         * CRITICAL: Disable interrupts before updating current_proc and
         * process states. A timer interrupt firing between current_proc
         * update and switch_to would see the new current_proc but still
         * be on the old stack, corrupting the saved context.
         */
        __asm__ __volatile__("cli");

        /* If the Process was Running - Mark it as Ready */
        if (proc_now->state == PROC_RUNNING) {
            proc_now->state = PROC_READY;
        }

        /* Mark the selected Process as Running */
        proc_next->state = PROC_RUNNING;
        current_proc = proc_next;

        /* Restore the incoming process's trace id so its sc survives across
         * this context switch. If proc_next was preempted/yielded mid-syscall
         * this is the syscall's unique id; otherwise it is the background id.
         *
         * Placed HERE - under cli, immediately after current_proc is updated -
         * so current_proc and log_trace_id flip together atomically. If set
         * before cli, a timer interrupt in that window would log under
         * proc_next's sc while current_proc is still proc_now (pid/sc mismatch,
         * i.e. two processes appearing to share one sc). The pre-cli
         * announcement logs above still ran as proc_now, so they correctly
         * carry proc_now's own sc. */
        log_trace_set(proc_next->trace_id);

        /** [START] Todo: move before switch_to */
        /* [todo] We have 1 TSS, its a good practice to have 1 per CPU */
        /* Update the TSS entry so if the process privilege switch from
         * user->kernel it can switch to that process kerel stack */
        tss_df.esp0 = (uint32_t)proc_next->kernel_stack_top;

        /* Load with the process page directory */
        if (!proc_next->page_directory_phys) {
            panik("yield: next process has no page directory");
        }
        paging_switch_address_space(proc_next->page_directory_phys);
        update_tss_cr3();
        /** [STOP] Todo: move before switch_to */

        /* Context Switch to New Process */
        switch_to(proc_now, proc_next);
        __asm__ __volatile__("sti");

        /* Testing interrupt after process switch */
        uint32_t eflags_afterswitch;
        __asm__ __volatile__("pushf"
                             "\n"
                             "pop %0"
                             : "=r"(eflags_afterswitch));

        debug_module(PROCESS_MGMT, "Resumed process: %s\n", current_proc->name);
        debug_module(PROCESS_MGMT, "EFLAGS After switch to: 0x%08x (IF=%s)\n",
                     PRINT_UINT32(eflags_afterswitch),
                     (eflags_afterswitch & 0x200) ? "enabled" : "disabled");

        /* Enable interrupts after context switch */
        if (!(eflags_afterswitch & 0x200)) {
            pr_warn("WARNING: Interrupts disabled after context switch "
                    "(process: %s). This should not happen.\n",
                    current_proc->name);
            // __asm__ __volatile__("sti");
        } else {
            debug_module(PROCESS_MGMT,
                         "Context switch properly restored IF bit.\n");
        }

        debug_module(PROCESS_MGMT, "TSS ESP0 value is 0x%08x\n", tss_df.esp0);

    } else {
        debug_module(PROCESS_MGMT, "No context switch needed - staying in %s\n",
                     proc_now ? proc_now->name : "NULL");
    }
}

void timer_interrupt_proc_handler(uint32_t tickcount) {
    pcb_t *p = wait_list_head;

    /* For each process update the timer */
    while (p) {
        pcb_t *next_p = p->next;
        if (p->sleep_ticks > 0) {
            p->sleep_ticks--;
        }

        /* Sleep timer has expired then enqueue to ready lit */
        if (p->sleep_ticks == 0) {
            proc_wakeup(p);
        }

        p = next_p;
    }

    /* Process premption scheduling */
    /* Premption - Kernel to context switch automatically on timer tick */
    if (current_proc != NULL && current_proc->state == PROC_RUNNING) {
        current_proc->timeslice_ticks--;
        pr_info("[TICK %u] %s: timeslice ticks = %u\n", PRINT_UINT32(tickcount),
                current_proc->name,
                PRINT_UINT32(current_proc->timeslice_ticks));
        if (current_proc->timeslice_ticks <= 0) {
            pr_info("%s out of timeslice! Switching...\n", current_proc->name);
            yield();
        }
    }
}

/******************************************
 * START: PROCESS ENTRY                   *
 * ****************************************/

void thread_entry_wrapper(void (*entry)(void *), void *arg) {
    tss_df.esp0 = (uint32_t)current_proc->kernel_stack_top;
    __asm__ __volatile__("sti");
    entry(arg);
    proc_exit();
}

/*******************************************
 * START: KERNEL ENTRY METHIOD             *
 *******************************************/
pcb_t *proc_create_kernel_main(const char *name) {
    pcb_t *kernel_proc = pcb_alloc();
    if (!kernel_proc) {
        return NULL;
    }

    kernel_proc->pid = next_pid++;
    kernel_proc->state = PROC_RUNNING;
    proc_set_type(kernel_proc, PROC_TYPE_BOOTSTRAP);

    memset(&kernel_proc->context, 0, sizeof(regs_context_t));

    uint32_t current_esp;
    __asm__ __volatile__("mov %%esp, %0" : "=r"(current_esp));

    kernel_proc->context.esp = current_esp;
    kernel_proc->context.eip = 0;
    kernel_proc->context.eflags = 0x202;

    kernel_proc->kernel_stack_base = (uint8_t *)KERNEL_STACK_BOTTOM_VIRT;
    kernel_proc->kernel_stack_top = (uint8_t *)KERNEL_STACK_TOP_VIRT;
    kernel_proc->kernel_stack_size =
        KERNEL_STACK_TOP_VIRT - KERNEL_STACK_BOTTOM_VIRT;

    kernel_proc->page_directory_virt = kernel_page_directory_virt;
    kernel_proc->page_directory_phys = kernel_page_directory_phys;

    strncpy(kernel_proc->name, name, PROC_NAME_MAX);
    kernel_proc->name[PROC_NAME_MAX - 1] = '\0';

    kernel_proc->parent = NULL;
    kernel_proc->timeslice_ticks = DEFAULT_TIMESLICE;

    kernel_proc->has_exited = 0;
    kernel_proc->exit_code = 0;

    enqueue_proc_list(kernel_proc);
    enqueue_ready(kernel_proc);

    current_proc = kernel_proc;

    return kernel_proc;
}

/**
 * Special exit for kernel main process - should trigger system shutdown
 */
void proc_kernel_main_exit(void) {
    pr_info("Kernel main process exiting - system shutdown\n");

    // In a production kernel, this might trigger:
    // - Graceful shutdown of all processes
    // - Filesystem sync
    // - Hardware shutdown

    // For now, just halt
    while (1) {
        __asm__ __volatile__("cli; hlt");
    }
}

static const char *proc_state_to_string(proc_state_t state) {
    switch (state) {
    case PROC_NEW:
        return "NEW";
    case PROC_READY:
        return "READY";
    case PROC_RUNNING:
        return "RUNNING";
    case PROC_WAITING:
        return "WAITING";
    case PROC_TERMINATED:
        return "TERMINATED";
    default:
        return "UNKNOWN";
    }
}

void print_proc_info(const pcb_t *proc) {
    if (!proc) {
        pr_info("Process is NULL\n");
        return;
    }

    if (proc->type == PROC_TYPE_USER) {
        KLOG_VERBOSE(
            "PROC",
            "[Identity] pid=%u name=%s type=%s state=%s\n\t"
            "[Lifecycle] exited=%u exit_code=0x%08x\n\t"
            "[Kernel Stack] base=0x%08x top=0x%08x size=0x%08x\n\t"
            "[Scheduling] timeslice=%u sleep_ticks=%u\n\t"
            "[Context] eip=0x%08x esp=0x%08x ebp=0x%08x "
            "eflags=0x%08x(IF=%s)\n\t"
            "[Linkage] parent=%s(pid=%u)\n\t"
            "[User Space] entry=0x%08x code_size=0x%08x stack_top=0x%08x "
            "stack_size=0x%08x\n\t"
            "[Address Space] pd_virt=0x%08x pd_phys=0x%08x\n",
            proc->pid, proc->name, proc_type_to_string(proc->type),
            proc_state_to_string(proc->state), proc->has_exited,
            proc->exit_code, proc->kernel_stack_base, proc->kernel_stack_top,
            proc->kernel_stack_size, proc->timeslice_ticks, proc->sleep_ticks,
            proc->context.eip, proc->context.esp, proc->context.ebp,
            proc->context.eflags, (proc->context.eflags & 0x200) ? "on" : "off",
            proc->parent ? proc->parent->name : "none",
            proc->parent ? proc->parent->pid : 0, proc->user_entry,
            proc->user_code_size, proc->user_stack_top, proc->user_stack_size,
            proc->page_directory_virt, proc->page_directory_phys);
    } else {
        KLOG_VERBOSE(
            "PROC",
            "[Identity] pid=%u name=%s type=%s state=%s\n\t"
            "[Lifecycle] exited=%u exit_code=0x%08x\n\t"
            "[Kernel Stack] base=0x%08x top=0x%08x size=0x%08x\n\t"
            "[Scheduling] timeslice=%u sleep_ticks=%u\n\t"
            "[Context] eip=0x%08x esp=0x%08x ebp=0x%08x "
            "eflags=0x%08x(IF=%s)\n\t"
            "[Linkage] parent=%s(pid=%u)\n",
            proc->pid, proc->name, proc_type_to_string(proc->type),
            proc_state_to_string(proc->state), proc->has_exited,
            proc->exit_code, proc->kernel_stack_base, proc->kernel_stack_top,
            proc->kernel_stack_size, proc->timeslice_ticks, proc->sleep_ticks,
            proc->context.eip, proc->context.esp, proc->context.ebp,
            proc->context.eflags, (proc->context.eflags & 0x200) ? "on" : "off",
            proc->parent ? proc->parent->name : "none",
            proc->parent ? proc->parent->pid : 0);
    }
}
