#include "scheduler.h"

pcb_t *ready_list_head = NULL;
pcb_t *ready_list_tail = NULL;

pcb_t *wait_list_head = NULL;
pcb_t *wait_list_tail = NULL;

void enqueue_ready (pcb_t *proc)
{
	proc->next = NULL;
	proc->prev = NULL;

	if (ready_list_tail)
	{
		/* If an entry is already present, add to the tail */
		ready_list_tail->next = proc;
		proc->prev = ready_list_tail;
		ready_list_tail = proc;
	}
	else
	{
		/* If first entry in the list */
		ready_list_head = proc;
		ready_list_tail = proc;
	}
}

void dequeue_ready (pcb_t *proc)
{
	if (proc->prev)
	{
		/* Adjust the process pointer prior to the one dequeued */
		proc->prev->next = proc->next;
	}
	else 
	{
		/* No process prior, asjust the head */
		ready_list_head = proc->next;
	}


	if (proc->next)
	{
		/* Adjust the process pointer next of the one dequeued */
		proc->next->prev = proc->prev;
	}
	else
	{
		/* No process next, asjust the tail */
		ready_list_tail = proc->prev;
	}

	proc->next = NULL;
	proc->prev = NULL;
}

void enqueue_wait (pcb_t *proc)
{
	proc->next = NULL;
	proc->prev = NULL;

	/* Insert the process at the end of the wait list */
	if (wait_list_tail)
	{
		/* Process already present in the wait list */
		wait_list_tail->next = proc;
		proc->prev = wait_list_tail;
		wait_list_tail = proc;
	}
	else
	{
		/* No Process present in the wait list */
		wait_list_head = proc;
		wait_list_tail = proc;
	}
}

void dequeue_wait (pcb_t *proc)
{
	if (proc->prev)
	{
		/* Process to be removed is not first entry */
		proc->prev->next = proc->next;
	}
	else
	{
		/* Process to be removed is the first entry */
		wait_list_head = proc->next;
	}

	if (proc->next)
	{
		/* Process to be removed is not last entry */
		proc->next->prev = proc->prev;
	}
	else
	{
		/* Process to be removed is the last entry */
		wait_list_tail = proc->prev;
	}
}
