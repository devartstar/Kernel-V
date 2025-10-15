#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stddef.h>
#include "proc.h"

/* For process ready to be executed */
extern pcb_t *ready_list_head;
extern pcb_t *ready_list_tail;

/* For sleeping or waiting process */
extern pcb_t *wait_list_head;
extern pcb_t *wait_list_tail;

/**
 * enqueue_ready - Add a process to the ready list
 * @proc - process to enqueue
 *
 * @return - void
 */
void enqueue_ready (pcb_t *proc);

/**
* dequeue_proc - Remove a process from the ready list
* @proc - process to dequeue
*
* @return - void
*/
void dequeue_ready (pcb_t *proc);

/**
 * enqueue_wait - Add a process to a wait queue.
 * @proc - pointer to the process to be enqueued.
 *
 * @return - void
 */
void enqueue_wait (pcb_t *proc);

/**
 * dequeue_wait - Removes a process from the wait queue.
 * @proc - pointer to the process to be dequeued.
 *
 * @return - void
*/
void dequeue_wait (pcb_t *proc);

#endif
