#ifndef PROC_H
#define PROC_H

#include <stdint.h>

#define PROC_NAME_MAX		16
#define KERNEL_STACK_SIZE	4096

//
// Process States
//
typedef enum
{
	PROC_NEW = 0,
	PROC_READY,
	PROC_RUNNING,
	PROC_WAITING,
	PROC_TERMINATED
} proc_state_t;

//
// Store the context of the registers here.
//
typedef struct regs_context
{
	uint32_t eip, esp, ebp;
	uint32_t eax, ebx, ecx, edx, esi, edi;
	uint32_t eflags;
} regs_context_t;

//
// PCB - Process control block
// Contains important info related to a specific process.
//
typedef struct pcb
{
	uint32_t		pid;		// process id
	proc_state_t	state;		// current state of the process
	regs_context_t	context;	// registers value
	uint8_t			*stack_base;// allocated stack base for cleanup
	uint8_t			*stack_ptr;	// current stack pointer
	char			name[PROC_NAME_MAX];

	// for linked list
	struct pcb		*parent;
	struct pcb		*next;
	struct pcb		*prev;
} pcb_t;

void proc_init (void);

pcb_t *proc_alloc (const char *name);

void proc_free (pcb_t *proc);

pcb_t *proc_find (uint32_t pid);

/**
 * proc_create - Create a new process that runs a function.
 * @entry pointer of entry function to run on executing the process
 *		  takes in a pointer of any type and returns void
 * @args arguments passed to the thread
 * @name string for debuging
*/
pcb_t *proc_create (void (*entry)(void*), void *args, const char* name);

extern pcb_t *proc_list_head;

extern pcb_t *current_proc;

#endif
