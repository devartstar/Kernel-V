#include "proc/proc.h"
#include "arch/x86/tss.h"
#include "core/debug.h"
#include "core/panik.h"
#include "lib/printk.h"
#include "lib/string.h"
#include "mm/pmm.h"
#include "mm/pool_alloc.h"
#include "proc/context_switch.h"
#include "proc/scheduler.h"
#include <stddef.h>

//  PID starts from 1
static uint32_t next_pid = 1;
pcb_t *current_proc = NULL;

void cleanup_terminated_processes(void) {
    pcb_t *p = ready_list_head;

    while (p) {
        pcb_t *next = p->next;

        if (p->state == PROC_TERMINATED && strcmp(p->name, "idle") != 0) {
            debug_module(PROCESS_MGMT, "Cleaning up terminated process: %s\n",
                         p->name);
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
            pr_info("[CLEANUP] Terminated Process\n");
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
    ready_list_head = NULL;
    wait_list_head = NULL;

    next_pid = 1;
    pcb_allocator_init();

    /* Create and IDLE process */
    pcb_t *idle = proc_create(idle_process, NULL, "idle");
    if (idle) {
        /* IDLE process should always be ready to run */
        debug_module(PROCESS_MGMT, "Created idle process with PID %d\n",
                     idle->pid);
    } else {
        panik("Failed creating IDLE process");
    }
}

pcb_t *proc_alloc(const char *name) {
    pcb_t *new_proc = pcb_alloc();
    if (!new_proc) {
        return NULL;
    }

    new_proc->pid = next_pid++;
    new_proc->state = PROC_NEW;
    memset(&new_proc->context, 0, sizeof(regs_context_t));
    new_proc->kernel_stack_base = NULL;
    new_proc->kernel_stack_top = NULL;
    new_proc->kernel_stack_size = NULL;
    strncpy(new_proc->name, name, PROC_NAME_MAX);
    new_proc->name[PROC_NAME_MAX - 1] = '\0';
    new_proc->parent = NULL;

    /* Add the new created proc to the ready queue */
    enqueue_ready(new_proc);

    return new_proc;
}

void proc_free(pcb_t *proc) {
    if (!proc) {
        return;
    }

    // Don't set state here - should already be TERMINATED
    // Don't dequeue here - should already be dequeued

    /* Free up the process kernel stack memory */
    if (proc->kernel_stack_base && proc->kernel_stack_size) {
        for (uint32_t offset = 0; offset < proc->kernel_stack_size;
             offset += PAGE_SIZE) {
            pmm_free_frame(
                (void *)((uint8_t *)proc->kernel_stack_base + offset));
        }
    }

    /* Free PCB */
    pcb_free(proc);
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
    void *kernel_stack_block = pmm_alloc_frame();
    if (!kernel_stack_block) {
        proc_free(proc);
        return NULL;
    }
    proc->kernel_stack_base = kernel_stack_block;
    proc->kernel_stack_size = KERNEL_STACK_SIZE;
    proc->kernel_stack_top = proc->kernel_stack_base + proc->kernel_stack_size;

    /* since stack grows downwards, stack pointer pointing to top of stack */
    uint32_t *stack_top = proc->kernel_stack_top;

    /*
     Update the stack to call the thread_entry_wrapper (entry, arg)
     When proc create it called instead of executing from the entry function
     It will start execution from thread_entry_wrapper which
     which takes entry func and args as its parameter.
    */

    /* push a fake return address (will never be used) */
    *(--stack_top) = 0;
    /* push the entry function to be used by thread_entry_wrapper */
    *(--stack_top) = (uint32_t)entry;
    /* push the argument pointer */
    *(--stack_top) = (uint32_t)arg;

    proc->context.esp = (uint32_t)(uintptr_t)stack_top;
    proc->context.eip = (uint32_t)(uintptr_t)thread_entry_wrapper;
    proc->context.ebp = 0;

    // Initialize EFLAGS with interrupts enabled
    proc->context.eflags =
        0x202; // IF (Interrupt Enable) bit set + reserved bit 1

    proc->state = PROC_READY;

    KLOG_VERBOSE("PROC",
                 "Process %s Created with kernel stack (top = 0x%08x, bottom = "
                 "0x%08x, size = 0x%08x)\n",
                 proc->name, proc->kernel_stack_top, proc->kernel_stack_base,
                 proc->kernel_stack_size);

    return proc;
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
    pcb_t *proc_next = NULL;

    /* Mark the process as Terminated */
    proc_now->state = PROC_TERMINATED;

    /* Remove the process from the Ready LIst */
    dequeue_ready(proc_now);

    yield();

    /* Should never reach here - the cleanup will happen later
       when the idle process calls cleanup_terminated_processes() */
    while (1) {
        __asm__ __volatile__("hlt");
    }
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
        if (proc_next && proc_next->state == PROC_READY) {
            return proc_next;
        }

        proc_next = proc_next ? proc_next->next : ready_list_head;

        if (proc_next == start_proc) {
            break;
        }
    } while (proc_next != proc_now);

    if (proc_now && proc_now->state == PROC_TERMINATED) {
        /*  Find and return idle process */
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
                 proc_now ? proc_now->pid : -1,
                 proc_now ? proc_now->name : "NULL");
        while (1)
            __asm__("hlt");
    }

    proc_next = scheduler_pick_next();

    /* Debug the new process contexts */
    if (proc_next) {
        pr_verbose(
            "DEBUG: Switching to %s: EIP=0x%08x ESP=0x%08x EFLAGS=0x%08x\n",
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
        /* If the Process is Terminated - Keep it Terminated */

        /* If the Process was Running - Mark it as Ready */
        if (proc_now->state == PROC_RUNNING) {
            proc_now->state = PROC_READY;
        }

        /* Mark the selected Process as Running */
        proc_next->state = PROC_RUNNING;
        current_proc = proc_next;

        debug_module(PROCESS_MGMT,
                     "About to switch: \n"
                     "\tPrev Process=%s (eflags=0x%lx) \n"
                     "\tNew Process=%s  (eflags=0x%lx) \n",
                     proc_now->name, proc_now->context.eflags, proc_next->name,
                     proc_next->context.eflags);

        /* Context Switch to New Process */
        switch_to(proc_now, proc_next);

        /* [todo] We have 1 TSS, its a good practice to have 1 per CPU */
        /* Update the TSS entry so if the process privilege switch from
         * user->kernel it can switch to that process kerel stack */
        tss_df.esp0 = (uint32_t)current_proc->kernel_stack_top;

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
            pr_warn("WARNING: Interrupts disabled after context switch! "
                    "Re-enabling...\n");
            __asm__ __volatile__("sti");
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
        pr_info("[TICK %lu] %s: timeslice ticks = %lu\n",
                PRINT_UINT32(tickcount), current_proc->name,
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

    strncpy(kernel_proc->name, name, PROC_NAME_MAX);
    kernel_proc->name[PROC_NAME_MAX - 1] = '\0';

    kernel_proc->parent = NULL;
    kernel_proc->timeslice_ticks = DEFAULT_TIMESLICE;

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
