#ifndef PROC_SIGNAL_H
#define PROC_SIGNAL_H

#include <stdint.h>

/* POSIX Compatible signal number */
#define SIGINT 2  /* ctrl-c : interrupt */
#define SIGQUIT 3 /* ctrl-\ : quit */
#define SIGKILL 9 /* unconditional kill */

#define SIG_NONE 0 /* sentinel: no signal pending */

/* sigpending is a BITMASK: bit (signo-1) set -> signal is pending.
 * signo is 1-based, so we shift by (signo-1) to use bit 0 for signal 1 */
#define SIG_BIT(signo) (1u << ((signo) - 1))
#define SIG_MASK_EMPTY (0u)

#endif /* PROC_SIGNAL_H */
