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

  @(#)pthread_asm.h	1.20 10/1/93

*/

#ifndef _pthread_pthread_asm_h
#define _pthread_pthread_asm_h

/*
 * sched attribute values
 */
#ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
#define SCHED_FIFO  0
#define SCHED_RR    1
#define SCHED_OTHER 2
#endif

/*
 * timer set modes
 */
#define ABS_TIME  0x01
#define REL_TIME  0x02
#define SYNC_TIME (ABS_TIME | REL_TIME)
#ifdef DEF_RR
#define RR_TIME   0x04
#define ANY_TIME  (SYNC_TIME | RR_TIME)
#else
#define ANY_TIME  SYNC_TIME
#endif
#define ALL_TIME  (0x08 | ANY_TIME)

/*
 * Thread status bits.
 */
#define	T_MAIN		0x1
#define	T_RETURNED	0x2
#define	T_DETACHED	0x4
#define T_RUNNING	0x8
#define T_BLOCKED	0x10
#define T_CONDTIMER     0x20
#define T_SIGWAIT       0x40
#define T_SYNCTIMER     0x80
#define T_SIGSUSPEND    0x100
#define T_CONTROLLED    0x200
#define T_INTR_POINT    0x400
#define T_ASYNCTIMER    0x800
#if STACK_CHECK && SIGNAL_STACK
#define T_LOCKED	0x1000
#endif
#ifdef IO
#define T_IO_OVER	0x2000
#endif

#ifndef NULL                          /* RFHH addition: test for defined */
#  define NULL            0
#endif

/*
 * Offset added to pc by ret and retl instruction on SPARC.
 * When a call instruction is issued, the address of the call
 * instruction is saved as the return address. When executing
 * a ret/retl a constant of 8 is added to the pc to return
 * to the instruction following the call *and* the daley slot.
 * Below, we want to jump to a function through the dispatcher
 * which uses ret/retl. Thus, we need to provide the address of
 * the function *minus* 8.
 */
#define RETURN_OFFSET 8

/*
 * If we have Priority Ceilings, then we have Priority Inheritance
 */
#ifdef _POSIX_THREADS_PRIO_PROTECT
#define _POSIX_THREADS_PRIO_INHERIT
#define NO_PRIO_INHERIT   0
#define PRIO_INHERIT      1
#define PRIO_PROTECT      2
#endif

#endif /*!_pthread_pthread_asm_h*/
