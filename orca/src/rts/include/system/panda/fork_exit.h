#ifndef __FORK_EXIT_H__
#define __FORK_EXIT_H__

#include "orca_types.h"

/* Module to handle forks and exits of Orca processes.
 */

extern void fe_start( void);

extern void fe_end( void);

extern void fe_await_termination(void);

/* VARARGS */
extern void DoFork(int cpu, prc_dscr *descr, void **argv);

extern void DoExit(prc_dscr *descr, void **argv);

#endif
