/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __FORK_EXIT_H__
#define __FORK_EXIT_H__

#include "orca_types.h"

/* Module to handle forks and exits of Orca processes.
 */

extern prc_dscr rts_proc_descr;

extern void fe_start( void);

extern void fe_end( void);

extern void fe_await_termination(void);

/* VARARGS */
extern void DoFork(int cpu, prc_dscr *descr, void **argv);

extern void DoExit(prc_dscr *descr, void **argv);

#endif
