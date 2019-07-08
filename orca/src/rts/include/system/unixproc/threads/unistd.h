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

  @(#)unistd.h	1.20 10/1/93

*/

#ifndef _pthread_unistd_h
#define _pthread_unistd_h

#include <unistd.h>

#ifndef _POSIX_THREADS
#define	_POSIX_THREADS
#define	_POSIX_THREAD_PRIORITY_SCHEDULING
#define _POSIX_THREAD_ATTR_STACKSIZE
#define _POSIX_THREADS_PRIO_PROTECT
#undef _POSIX_THREADS_PROCESS_SHARED
#undef _POSIX_THREADS_PRIO_INHERIT
#undef _POSIX_REENTRANT_FUNCTIONS
#endif

#endif /*!_pthread_unistd_h*/
