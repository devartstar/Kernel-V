#ifndef PROC_H
#define PROC_H

#include "fs/ramfs.h"
#include "proc/signal.h"
#include <stdint.h>

#define PROC_NAME_MAX 16
#define KERNEL_STACK_SIZE 4096
#define DEFAULT_TIMESLICE 10
#define KERNEL_MAIN_TIMESLICE 20

#define PROCESS_MAX_FDS 32
#define PROCESS_FIRST_NORMAL_FD 3

//
// Process Types
//
typedef enum {
    PROC_TYPE_UNASSIGNED = 0,
    PROC_TYPE_BOOTSTRAP,
    PROC_TYPE_IDLE,
    PROC_TYPE_KERNEL,
    PROC_TYPE_USER
} proc_type_t;

//
//  Process States
//
typedef enum {
    PROC_NEW = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_WAITING,
    PROC_TERMINATED
} proc_state_t;

//
// Process reason for waiting
//
typedef enum {
    PROC_WAIT_NONE = 0,
    PROC_WAIT_SLEEP,
    PROC_WAIT_CONSOLE_INPUT
} proc_wait_reason_t;

//
// Process wait information
//
typedef struct proc_wait_info {
    proc_wait_reason_t wait_reason;
    uint32_t wait_tick_count;

    /**
     * wait_channel - opaque identity of the object process is blocked upon.
     * NULL for reason only wait. (sleep, legacy console).
     *
     * Object waiters store the address of their wait objects. Producers can
     * wake up exactly those who are blocked on the objects.
     */
    void *wait_channel;
} proc_wait_info_t;

//
//  Store the context of the registers here.
//
typedef struct regs_context {
    uint32_t eip, esp, ebp;
    uint32_t eax, ebx, ecx, edx, esi, edi;
    uint32_t eflags;
} regs_context_t;

/**
 * PCB - Process control block
 * Contains important info related to a specific process.
 * @pid Process Id
 * @state Current state of the process
 * @context Register value to store for context switch
 * @name Name of the process
 * @kernel_stack_base Kernel stack bottom address for the process
 * @kernel_stack_top Kernel stack top address for the process
 * @kernel_stack_size Kernel stack size for the process
 * @proc_wait_info Process reason for wait and ticks to wait for
 * @timeslice_ticks Cycles for the process to execute before switch
 * @user_stack_top User stack top address for the process
 * @user_stack_size User stack size for the process
 * @parent Pointer to the parent process PCB struct
 * @next Pointer to the next PCB struct in the linked list
 * @prev Pointer to the previous PCB struct in the linked list
 */
typedef struct pcb {
    uint32_t pid;
    proc_state_t state;
    proc_type_t type;
    regs_context_t context;
    char name[PROC_NAME_MAX];

    /* Lifecycle */
    uint8_t has_exited;
    int32_t exit_code;

    /* When set, the idle reaper (cleanup_terminated_processes) must not free
     * this PCB even after it has terminated. Used by observers (e.g. the
     * syscall integration harness) that hold a raw PCB pointer and need to read
     * the exit code after termination; the observer clears this flag once it
     * has snapshotted what it needs so the PCB can be reclaimed. */
    uint8_t reap_blocked;

    /* Kernel Stack */
    uint8_t *kernel_stack_base;
    uint8_t *kernel_stack_top;
    uint32_t kernel_stack_size;

    /* Scheduling */
    uint32_t timeslice_ticks;

    /* reason for process to get to wait state */
    proc_wait_info_t wait_info;

    /* 0 = none; else signal number to act on at wakeup */
    uint32_t sigpending;

    /* Log correlation (sc=) tracing.
     * trace_id: id this process is currently running under (restored by
     *           yield() on context switch so it survives yields/preemption).
     * trace_id_saved: background id stashed while a syscall runs under a fresh
     *           per-operation id. */
    uint32_t trace_id;
    uint32_t trace_id_saved;

    /* User Space */
    uint32_t user_entry;
    uint32_t user_stack_top;
    uint32_t user_stack_size;
    uint32_t user_code_size;

    /* Address space */
    uint32_t *page_directory_virt;
    uint32_t page_directory_phys;

    /* File descriptor table */
    vfs_file_t *fds[PROCESS_MAX_FDS];

    /* Process tree */
    struct pcb *parent;

    /* All process linkage */
    struct pcb *all_next;
    struct pcb *all_prev;

    /* Ready queue linkage */
    struct pcb *next;
    struct pcb *prev;
} pcb_t;

/* List of all the processes across all states */
extern pcb_t *proc_list_head;
extern pcb_t *proc_list_tail;

void proc_init(void);

pcb_t *proc_alloc(const char *name);

void enqueue_proc_list(pcb_t *proc);

void dequeue_proc_list(pcb_t *proc);

/**
 * proc_cleanup_kernel - cleanup kernel level process
 * proc - kernel mode proc for cleanup
 *
 * @return void
 */
void proc_cleanup_kernel(pcb_t *proc);

/**
 * proc_cleanup_user - cleanup user level process
 * proc - usermode proc for cleanup
 *
 * @return void
 */
void proc_cleanup_user(pcb_t *proc);

void proc_free(pcb_t *proc);

pcb_t *proc_find(uint32_t pid);

/**
 * proc_create - Create a new process that runs a function.
 * @entry pointer of entry function to run on executing the process
 *		  takes in a pointer of any type and returns void
 * @args arguments passed to the thread
 * @name string for debuging
 */
pcb_t *proc_create(void (*entry)(void *), void *args, const char *name);

/**
 * proc_set_type - Set the process type for the newly created process
 * @proc pointer to the pcb struc to set the type
 * @type of the process, below are the different types:
 * PROC_TYPE_BOOTSTAP = only for kernel main process
 * PROC_TYPE_IDLE = only for idle task
 * PROC_TYPE_KERNEL = for normal kernel threads
 * PROC_TYPE_USER = any process that will execute usermode code
 */
void proc_set_type(pcb_t *proc, proc_type_t type);

/**
 * proc_mark_ready - Mark a fully-initialized process as schedulable.
 * Call this only after all setup (kernel stack, user blob, etc.) is complete.
 * @proc pointer to the pcb to mark ready
 */
void proc_mark_ready(pcb_t *proc);

/*
 * proc_type_to_string - enum to string conversion for the given process type
 * @type of process
 *
 * @returns the string format for the current proc type
 */
const char *proc_type_to_string(proc_type_t type);

/**
 * proc_wait_sleep - Puts the current running process to sleep till next tick.
 * @ticks - count of cpu intervals for process to sleep.
 *
 * @return - void
 */
void proc_wait_sleep(uint32_t ticks);

/**
 * proc_wait_console_input - Puts the current running process to sleep till
 * console input is available
 */
void proc_wait_console_input(void);

/**
 * @proc_wait_prepare_console_input - similar to proc_wait_console_input without
 * yieling. sets up wait reason and ticks and removed from ready queue to add to
 * wait queue.
 */
void proc_wait_prepare_console_input(void);

/**
 * @proc_wait_prepare_on - object based wait preperation. (no yield)
 * marks the current process waiting on channel object. records the wait reason
 * moves the process from ready to wait queue.
 * process woken selectively by proc_wakeup_all_on based on @channel
 */
void proc_wait_prepare_on(proc_wait_reason_t reason, void *channel);

/**
 * proc_wakeup - Wakes up a sleeping process and adds to ready queue.
 * @proc - process to wake up.
 *
 * @return - void
 */
void proc_wakeup(pcb_t *proc);

/**
 * proc_wakeup_one_reason - wakes up one of the waiting process under a
 * specific reason
 *
 * @reason - reason for wait
 */
void proc_wakeup_one_reason(uint32_t reason);

/**
 * proc_wakeup_all_on - wakeup every process blocked on the wait object
 * Object based counterpart of proc_wakeup_one_reason.
 * @channel - object the process are waiting on
 */
void proc_wakeup_all_on(void *channel);

/**
 * proc_exit - Exits and cleanup the process
 *
 * @return - void
 */
void proc_exit(void);

/*
 * proc_is_reclaimable - If the process memory should be freed once terminated
 * proc - process to check if it is reclaimable
 *
 * @return void
 */
int proc_is_reclaimable(const pcb_t *proc);

/**
 * proc_is_special - check if process should not be freed
 * proc - process to check if it is special
o*
 * @return void
 */
int proc_is_special(const pcb_t *proc);

/**
 * proc_mark_terminated - mark the process as terminated when it exits
 * proc - process to be marked as terminated
 * exit_code - exit code for the terminated process
 *
 * @return void
 */
void proc_mark_terminated(pcb_t *proc, int32_t exit_code);

/**
 * proc_is_runnable - checks if the process is eligible to be picked by
 * scheduler
 * proc - process to check if it can run next
 *
 * @return uint8_t - 1 if runnable else 0
 */
uint8_t proc_is_runnable(const pcb_t *proc);

/**
 * proc_bootstrap_handoff - remove the bootstrap proc from further scheduling
 *
 * @return void
 */
void proc_bootstrap_handoff(void);

/**
 * thread_entry_wrapper - Wrapper for process entry and exit.
 * For every new thread, sets the EIP here.
 * Push entry and args to the stack of the new process.
 *
 * @entry - pointer to the entry method
 * @args  - pointer to the args list for the entry method
 *
 * @return - void
 */
void thread_entry_wrapper(void (*entry)(void *), void *arg);

/**
 * scheduler_pick_next - Picks a process ready to execute from the process list
 *
 * 1. Try to pick up a process in READY state.
 * 2. No such process - check for the current process.
 * 3. If current process is TERMINATED. Schedule an IDLE process.
 * 4.
 *
 * @returns the pointer to the pcb memory block
 */
pcb_t *scheduler_pick_next(void);

/**
 * yeild - Find the next process ready to run from scheduler
 * Coxtext Switch to the next process
 */
void yield(void);

/*
 * timer_interrupt_proc_handler - Handels an interrupt then process sleep time
 * becomes 0.
 * @tickcount current timer tick
 *
 * @return - void
 */
void timer_interrupt_proc_handler(uint32_t tickcount);

/**
 * Create the kernel main process
 * @name - name of the process
 *
 * @return pcb_t* - pointer to the pcb of the process
 */
pcb_t *proc_create_kernel_main(const char *name);

/**
 * Exit method for kernel main
 */
void proc_kernel_main_exit(void);

/**
 * Log the Process info for debugging
 * @proc - process to log the info for
 */
void print_proc_info(const pcb_t *proc);

/**
 * Act for any signal pending for the current process.
 * Runs in the process's OWN context at a safe point (never in IRQ)
 * Returns 1 if the signal was acted upon (caller may need to bail out)
 * 0 if nothing was pending
 */
int proc_handle_pending_signals(void);

/**
 * proc_signal_channel - deliver a signal to every process parked on the
 * channel. they wake them so they act on it at their safe point.
 *
 * called from IRQ context (canon INTR/QUIT detection). We only mark the signal
 * + wake here; the target consumes the bit in proc_handle_pending_signals()
 * after it resumes.
 */
void proc_signal_channel(void *channel, int signo);

extern pcb_t *current_proc;

#endif
