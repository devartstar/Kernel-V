#include "proc/scheduler.h"
#include "lib/printk.h"

pcb_t *ready_list_head = NULL;
pcb_t *ready_list_tail = NULL;

pcb_t *wait_list_head = NULL;
pcb_t *wait_list_tail = NULL;

void enqueue_ready(pcb_t *proc) {
    proc->next = NULL;
    proc->prev = NULL;

    if (ready_list_tail) {
        /* If an entry is already present, add to the tail */
        ready_list_tail->next = proc;
        proc->prev = ready_list_tail;
        ready_list_tail = proc;
    } else {
        /* If first entry in the list */
        ready_list_head = proc;
        ready_list_tail = proc;
    }

    KLOG_VERBOSE(
        "SCHEDULER",
        "Enqueue to ready list successful: name=%s (pid=%u, type=%s)\n",
        proc->name, proc->pid, proc_type_to_string(proc->type));
}

void dequeue_ready(pcb_t *proc) {
    /* Guard against double-dequeue: if the process is not in the ready list,
       skip. A process with no prev/next that isn't the head is not linked. */
    if (!proc->prev && !proc->next && proc != ready_list_head) {
        return;
    }

    if (proc->prev) {
        /* Adjust the process pointer prior to the one dequeued */
        proc->prev->next = proc->next;
    } else {
        /* No process prior, asjust the head */
        ready_list_head = proc->next;
    }

    if (proc->next) {
        /* Adjust the process pointer next of the one dequeued */
        proc->next->prev = proc->prev;
    } else {
        /* No process next, asjust the tail */
        ready_list_tail = proc->prev;
    }

    proc->next = NULL;
    proc->prev = NULL;

    KLOG_VERBOSE(
        "SCHEDULER",
        "Dequeue from ready list successful: name=%s (pid=%u, type=%s)\n",
        proc->name, proc->pid, proc_type_to_string(proc->type));
}

void enqueue_wait(pcb_t *proc) {
    proc->next = NULL;
    proc->prev = NULL;

    /* Insert the process at the end of the wait list */
    if (wait_list_tail) {
        /* Process already present in the wait list */
        wait_list_tail->next = proc;
        proc->prev = wait_list_tail;
        wait_list_tail = proc;
    } else {
        /* No Process present in the wait list */
        wait_list_head = proc;
        wait_list_tail = proc;
    }

    KLOG_VERBOSE("SCHEDULER",
                 "Enqueue to wait list successful: name=%s (pid=%u, type=%s)\n",
                 proc->name, proc->pid, proc_type_to_string(proc->type));
}

void dequeue_wait(pcb_t *proc) {
    /* Guard against double-dequeue */
    if (!proc->prev && !proc->next && proc != wait_list_head) {
        return;
    }

    if (proc->prev) {
        /* Process to be removed is not first entry */
        proc->prev->next = proc->next;
    } else {
        /* Process to be removed is the first entry */
        wait_list_head = proc->next;
    }

    if (proc->next) {
        /* Process to be removed is not last entry */
        proc->next->prev = proc->prev;
    } else {
        /* Process to be removed is the last entry */
        wait_list_tail = proc->prev;
    }

    proc->next = NULL;
    proc->prev = NULL;

    KLOG_VERBOSE(
        "SCHEDULER",
        "Dequeue from weady list successful: name=%s (pid=%u, type=%s)\n",
        proc->name, proc->pid, proc_type_to_string(proc->type));
}
