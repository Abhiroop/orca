/* Copyright (C) 1992, the Florida State University
   Distributed by the Florida State University under the terms of the
   GNU Library General Public License.

This file is part of Pthreads.

Pthreads is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation (version 2).

Pthreads is distributed "AS IS" in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with Pthreads; see the file COPYING.  If not, write
to the Free Software Foundation, 675 Mass Ave, Cambridge,
MA 02139, USA.

Report problems and direct all questions to:

  pthreads-bugs@ada.cs.fsu.edu

  @(#)signal.h	1.20 10/1/93

*/

#ifndef _pthread_signal_h
#define _pthread_signal_h

#ifndef	__signal_h

#ifdef LOCORE
#undef LOCORE
#endif

#include "stdtypes.h"
#include <signal.h>

/*
 * Just in case someone has a modified signal.h where __signal_h is not defined
 */
#define	__signal_h

#define NNSIG     NSIG+1

#define CLOCK_REALTIME 1

#define PTHREAD_INTR_ENABLE       SIG_UNBLOCK
#define PTHREAD_INTR_DISABLE      SIG_BLOCK
#define PTHREAD_INTR_CONTROLLED   0
#define PTHREAD_INTR_ASYNCHRONOUS 1

union sigval {
  int u0;
};

struct siginfo {
  int si_signo;
  int si_code;
  union sigval si_value;
};

/*
 * This defines the implementation-dependent context structure provided
 * as the third parameter to user handlers installed by sigaction().
 * It should be a copy of the first part of the UNIX sigcontext structure.
 * The second half should not be accessed since it is only present if
 * a _sigtramp instance is present right below the user handler on the
 * thread's stack.
 */
struct context_t {
  int sc_onstack; /* ignored */
  int sc_mask;    /* per-thread signal mask to be restored */
  int sc_sp;      /* stack pointer to be restored */
  int sc_pc;      /* program counter to be restored */
  int sc_npc;     /* next pc, only used if _sigtramp present
                   * on thread's stack, ignored o.w.
		   * should usually be pc+4
		   */
};

#define SA_SIGINFO SA_NOCLDSTOP<<1

#endif /*	__signal_h */

#endif /*!_pthread_signal_h*/
