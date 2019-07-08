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

  @(#)signal_internals.h	1.20 10/1/93

*/

#ifndef _pthread_signal_internals_h
#define _pthread_signal_internals_h

#ifndef	__signal_h

#include "signal.h"

#define SIGCANCEL NSIG
#define	cantmask  (sigmask(SIGKILL)|sigmask(SIGSTOP)|sigmask(SIGCANCEL))

#define DIRECTED_AT_THREAD (struct sigcontext *) NULL

#define TIME_SLICE 20000

/*
 * MINUS_TIME only works if src1 > src2
 */
#define MINUS_TIME(dst, src1, src2) \
  MACRO_BEGIN \
    if (src2.tv_usec > src1.tv_usec) { \
      dst.tv_sec = src1.tv_sec - src2.tv_sec - 1; \
      dst.tv_usec = (src1.tv_usec - src2.tv_usec) + 1000000; \
    } \
    else { \
      dst.tv_sec = src1.tv_sec - src2.tv_sec; \
      dst.tv_usec = src1.tv_usec - src2.tv_usec; \
    } \
  MACRO_END

#define PLUS_TIME(dst, src1, src2) \
  MACRO_BEGIN \
    if (1000000 - src2.tv_usec <= src1.tv_usec) { \
      dst.tv_sec = src1.tv_sec + src2.tv_sec + 1; \
      dst.tv_usec = -1000000 + src1.tv_usec + src2.tv_usec; \
    } \
    else { \
      dst.tv_sec = src1.tv_sec + src2.tv_sec; \
      dst.tv_usec = src1.tv_usec + src2.tv_usec; \
    } \
  MACRO_END

#define GT_TIME(t1, t2) \
      (t1.tv_sec > t2.tv_sec || \
       (t1.tv_sec == t2.tv_sec && \
	t1.tv_usec > t2.tv_usec))

#define GTEQ_TIME(t1, t2) \
      (t1.tv_sec > t2.tv_sec || \
       (t1.tv_sec == t2.tv_sec && \
	t1.tv_usec >= t2.tv_usec))

#define LE0_TIME(t1) \
      (t1.tv_sec < 0 || \
       (t1.tv_sec == 0 && \
	t1.tv_usec <= 0))

#define P2U_TIME(dst, src) \
  MACRO_BEGIN \
  dst.tv_sec = src.tv_sec; \
  dst.tv_usec = src.tv_nsec / 1000; \
  MACRO_END

#define U2P_TIME(dst, src) \
  MACRO_BEGIN \
  dst.tv_sec = src.tv_sec; \
  dst.tv_nsec = src.tv_usec * 1000; \
  MACRO_END

#endif /*	__signal_h */

#endif /*!_pthread_signal_internals_h*/
