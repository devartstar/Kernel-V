#ifndef PROC_H
#define PROC_H

#include <stdint.h>

#define PROC_NAME_MAX 16
#define KERNEL_STACK_SIZE 4096
#define DEFAULT_TIMESLICE 10
#define KERNEL_MAIN_TIMESLICE 20

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
 * @sleep_ticks Cycles for the process to sleep
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
    regs_context_t context;
    char name[PROC_NAME_MAX];

    /* Kernel Stack */
    uint8_t *kernel_stack_base;
    uint8_t *kernel_stack_top;
    uint32_t kernel_stack_size;

    /* Scheduling */
    uint32_t sleep_ticks;
    uint32_t timeslice_ticks;

    /* User Space */
    uint32_t user_stack_top;
    uint32_t user_stack_size;

    /* Process tree */
    struct pcb *parent;
    struct pcb *next;
    struct pcb *prev;
} pcb_t;

void proc_init(void);

pcb_t *proc_alloc(const char *name);

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
 * proc_sleep - Puts the current running process to sleep till next tick.
 * @ticks - count of cpu intervals for process to sleep.
 *
 * @return - void
 */
void proc_sleep(uint32_t ticks);

/**
 * proc_wakeup - Wakes up a sleeping process and adds to ready queue.
 * @proc - process to wake up.
 *
 * @return - void
 */
void proc_wakeup(pcb_t *proc);

/**
 * proc_exit - Exits and cleanup the process
 *
 * @return - void
 */
void proc_exit(void);

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

extern pcb_t *current_proc;

#endif
