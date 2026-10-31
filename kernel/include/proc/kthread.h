#ifndef KTHREAD_H
#define KTHREAD_H

#include "proc/proc.h"
#include <stdbool.h>

typedef int (*kthread_fn_t)(void *arg);

/**
 * @kthread_create creates a kernel thread which is not runnable yet
 * Joinable: the owner must call kthread_stop() exactly once
 *
 * @fn ref to the startup routine for the thread
 * @args ref to the arguments passed to the startup routine
 * @name name of the thread
 *
 * @returns ref to the thread pcb block
 */
pcb_t *kthread_create(kthread_fn_t fn, void *arg, const char *name);

/**
 * @kthread_run creates and run the thread
 * @kthread_create + run the thread
 */
pcb_t *kthread_run(kthread_fn_t fn, void *arg, const char *name);

/**
 * @kthread_stop asks the thread to stop, wake if sleeping, and block until it
 * exits.
 * @returns thread startup routine exit result.
 */
int kthread_stop(pcb_t *t);

/**
 * @kthread_should_stop checks if the thread needs to be stoppped.
 * Invoked from within the thread context
 */
bool kthread_should_stop(void);

#endif /* KTHREAD_h */
