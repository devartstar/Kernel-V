#include <stddef.h>
#include "lib/printk.h"
#include "lib/string.h"
#include "mm/pool_alloc.h"
#include "proc/proc.h"
#include "proc/context_switch.h"
#include "proc/scheduler.h"
#include "core/debug.h"

// PID starts from 1
static uint32_t next_pid = 1;
pcb_t *current_proc = NULL;

void cleanup_terminated_processes(void) {
    pcb_t *p = ready_list_head;
    
    while (p) {
        pcb_t *next = p->next;
        
        if (p->state == PROC_TERMINATED && strcmp(p->name, "idle") != 0) {
            debug_module(PROCESS_MGMT, "Cleaning up terminated process: %s\n", p->name);
            proc_free(p);
        }
        
        p = next;
    }
}

static void idle_process(void *arg) {
    static int idle_count = 0;
    
    while (1) {
        debug_module(PROCESS_MGMT, "IDLE process running (count: %d)\n", idle_count++);
        
        // Every 10 idle cycles, clean up terminated processes
        if (idle_count % 10 == 0) {
            cleanup_terminated_processes();
        }
        
        // Delay
        for (volatile int i = 0; i < 1000000; i++);
        
        yield();
    }
}

// -----------------------------------
// START: POOL ALLOCATOR for PCB
// -----------------------------------

static pool_allocator_t pcb_pool;

void pcb_allocator_init () 
{
	pool_init (&pcb_pool, sizeof(pcb_t));
}

pcb_t* pcb_alloc ()
{
	return (pcb_t*)pool_alloc (&pcb_pool);
}

void pcb_free (pcb_t* pcb)
{
	pool_free (&pcb_pool, pcb);
}

// --------------------------------------
// END: POOL ALLOCATOR for PCB 
// --------------------------------------

void proc_init (void)
{
	ready_list_head = NULL;
	next_pid = 1;
	pcb_allocator_init ();

    /* IDLE PROCESS LOGIC
	pcb_t *idle = proc_create(idle_process, NULL, "idle");
    if (idle) {
        current_proc = idle;  // Set as current process
        idle->state = PROC_RUNNING;  // Mark as running
        debug_module(PROCESS_MGMT, "Created idle process with PID %d\n", idle->pid);
    } else {
        debug_module(PROCESS_MGMT, "ERROR: Failed to create idle process!\n");
    }
    */
}

pcb_t *proc_alloc (const char *name)
{
	pcb_t *new_proc = pcb_alloc ();
	if (!new_proc)
	{
		return NULL;
	}

	new_proc->pid = next_pid++;
	new_proc->state = PROC_NEW;
	memset (&new_proc->context, 0, sizeof(regs_context_t));
	new_proc->stack_base = NULL;
	new_proc->stack_ptr = NULL;
	strncpy (new_proc->name, name, PROC_NAME_MAX);
	new_proc->name[PROC_NAME_MAX-1] = '\0';
	new_proc->parent = NULL;

    /* Add the new created proc to the ready queue */
    enqueue_ready (new_proc);

	return new_proc;
}

void proc_free (pcb_t *proc)
{
	if (!proc)
	{
		return;
	}

    /* Clean-up the exiting process */
    proc->state = PROC_TERMINATED;

    /* Remove the proc from the ready queue */
    dequeue_ready (proc);

    /* Free up the process stack memory */
	if (proc->stack_base)
	{
		pmm_free_frame (proc->stack_base);
	}

	/* Free PCB */
	pcb_free (proc);
}

pcb_t *proc_find (uint32_t pid)
{
	for (pcb_t *p = ready_list_head; p; p = p->next)
	{
		if (p->pid == pid)
		{
			return p;
		}

	}
	return NULL;
}

pcb_t *proc_create (void (*entry)(void*), void *arg, const char *name)
{
	/* create a pcb for the process */
	pcb_t *proc = proc_alloc (name);
	if (!proc)
	{
		/* could not allocate memory to pcb */
		return NULL;
	}

	/* allocate stack to process */
	void *stack = pmm_alloc_frame();
	if (!stack)
	{
		proc_free (proc);
		return NULL;
	}
	proc->stack_base = stack;

	/* since stack grows downwards, stack pointer should point to top of stack
	*/
	uint32_t *stack_top = (uint32_t *)((uint8_t *)stack + KERNEL_STACK_SIZE);

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

	proc->context.esp = (uint32_t *)stack_top;
	proc->context.eip = (uint32_t *)thread_entry_wrapper;
	proc->context.ebp = 0;

	proc->state = PROC_READY;

	return proc;
}

void proc_sleep (uint32_t ticks)
{
    current_proc->state = PROC_WAITING;
    current_proc->sleep_ticks = ticks;

    dequeue_ready (current_proc);
    enqueue_wait  (current_proc);

    /* Current process is put to sleep, schedule a new process to run */
    yield();
}

void proc_wakeup (pcb_t *proc)
{
    dequeue_wait (proc);

    proc->state = PROC_READY;
    proc->sleep_ticks = 0;

    enqueue_ready (proc);
}

void proc_exit (void)
{
    pcb_t *proc_now = current_proc;
    pcb_t *proc_next = NULL;

    proc_free (proc_now);

    proc_next = scheduler_pick_next ();

    current_proc = proc_next;

    /* Context Switch to the new scheduled process */
    switch_to (NULL, proc_next);

    /* Ideally should never reach here as procees state is terminated */
    while (1) { __asm__ __volatile__("hlt"); }
}

pcb_t *scheduler_pick_next (void)
{
    pcb_t *proc_now = current_proc;
    pcb_t *proc_next = NULL;

    if (!current_proc) {
        return ready_list_head;
    }

    /* Round-robin scheduling: iterate through all processes starting from next */
    pcb_t *start_proc = proc_now->next ? proc_now->next : ready_list_head;
    proc_next = start_proc;

    // Look for a READY process (skip SLEEPING and TERMINATED processes)
    do {
        if (proc_next && proc_next->state == PROC_READY) {
            return proc_next;
        }
        
        proc_next = proc_next ? proc_next->next : ready_list_head;
        
        if (proc_next == start_proc) {
            break;
        }
    } while (proc_next != proc_now);

    // If current process is TERMINATED, definitely switch to idle
    if (proc_now && proc_now->state == PROC_TERMINATED) {
        // Find and return idle process
        for (pcb_t *p = ready_list_head; p; p = p->next) {
            if (strcmp(p->name, "idle") == 0) {
                return p;
            }
        }
    }

    // If no READY process found, return the idle process
    for (pcb_t *p = ready_list_head; p; p = p->next) {
        if (strcmp(p->name, "idle") == 0) {
            return p;
        }
    }
    
    return proc_now;
}

void yield (void)
{
    pcb_t *proc_now = current_proc;
    pcb_t *proc_next = NULL;

    debug_module(PROCESS_MGMT, "\n=== YIELD DEBUG ===\n");
    debug_module(PROCESS_MGMT, "Current process: %s (state: %d)\n", proc_now ? proc_now->name : "NULL", proc_now ? proc_now->state : -1);

    // Print the list of PCB in the process list
    for (pcb_t *p = ready_list_head; p; p = p->next) 
    {
        debug_module(PROCESS_MGMT, "PCB[%s]: EIP=0x%08x ESP=0x%08x state=%d\n", p->name, (uint32_t)p->context.eip, (uint32_t)p->context.esp, p->state);
    }

    proc_next = scheduler_pick_next ();

    pr_info("Selected next process: %s\n", proc_next ? proc_next->name : "NULL");

    if (proc_next && proc_next != proc_now)
    {
        debug_module(PROCESS_MGMT, "Switching from %s to %s\n", proc_now->name, proc_next->name);

        // Don't mark TERMINATED processes as READY
        if (proc_now->state == PROC_RUNNING) 
        {
            proc_now->state = PROC_READY;
        }
        // If proc_now is TERMINATED, leave it as TERMINATED

        // Mark next as running
        proc_next->state = PROC_RUNNING;
        current_proc = proc_next;

        // Check if this is the first switch from idle (which was never properly started)
        if (proc_now && strcmp(proc_now->name, "idle") == 0 && 
            proc_now->context.eip == (uint32_t)idle_process) {
            
            debug_module(PROCESS_MGMT, "First switch from unstarted idle process - jumping directly\n");
            
            // Jump directly to the process without saving idle context
            __asm__ __volatile__ (
                "mov %0, %%esp\n\t"          // Load process stack
                "push $0\n\t"                // Push argument (NULL)
                "jmp *%1"                    // Jump to process entry point
                :
                : "r"((uint32_t)proc_next->context.esp),
                  "r"((uint32_t)proc_next->context.eip)
                : "memory"
            );
            
            // Should never reach here
            debug_module(PROCESS_MGMT, "ERROR: Returned from direct jump!\n");
        } else {
            // Normal context switch between processes
            switch_to (proc_now, proc_next);
        }

        // Execution resumes from here when switch back
        debug_module(PROCESS_MGMT, "Resumed process: %s\n", current_proc->name);
    }
    else
    {
        debug_module(PROCESS_MGMT, "No context switch needed - staying in %s\n", proc_now ? proc_now->name : "NULL");
    }
}

void timer_interrupt_proc_handler (void)
{
    pcb_t *p = wait_list_head;

    /* For each process update the timer */
    while (p)
    {
        pcb_t *next_p = p->next;
        if (p->sleep_ticks > 0)
        {
            p->sleep_ticks--;
        }

        /* Sleep timer has expired then enqueue to ready lit */
        if (p->sleep_ticks == 0)
        {
            proc_wakeup (p);
        }

        p = next_p;
    }

    /* 
     Process premption scheduling
     yeild();
    */
}

void thread_entry_wrapper (void (*entry)(void *), void *arg)
{
    entry (arg);
    proc_exit ();
}
