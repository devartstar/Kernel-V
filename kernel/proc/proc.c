#include "pool_alloc.h"
#include "proc.h"
#include <stddef.h>
#include <string.h>

// PID starts from 1
static uint32_t next_pid = 1;
pcb_t			*proc_list_head = NULL;
pcb_t			*current_proc = NULL;

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
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
	proc_list_head = NULL;
	next_pid = 1;
	pcb_allocator_init ();
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
	new_proc->next = proc_list_head;
	new_proc->prev = NULL;

	if (proc_list_head)
	{
		proc_list_head->prev = new_proc;
	}

	proc_list_head = new_proc;

	return new_proc;
}

void proc_free (pcb_t *proc)
{
	if (!proc)
	{
		return;
	}

	/* Unlink from the list */
	if (proc->prev)
	{
		proc->prev->next = proc->next;
	}
	else
	{
		proc_list_head = proc->next;
	}

	if (proc->next)
	{
		proc->next->prev = proc->prev;
	}

	/* Free Stack */
	if (proc->stack_base)
	{
		pmm_free_frame (proc->stack_base);
	}

	/* Free PCB */
	pcb_free (proc);
}

pcb_t *proc_find (uint32_t pid)
{
	for (pcb_t *p = proc_list_head; p; p = p->next)
	{
		if (p->pid == pid)
		{
			return p;
		}

	}
	return NULL;
}

pcb_t *proc_create (void (*entry)(void*), void *args, const char *name)
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

	proc->context.esp = (uint32_t *)stack_top;
	proc->context.eip = (uint32_t *)entry;
	proc->context.ebp = 0;

	proc->state = PROC_READY;

	return proc;
}
