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

  @(#)limits.h	1.20 10/1/93

*/

#ifndef _pthread_limits_h
#define _pthread_limits_h

#include <limits.h>

#ifndef _POSIX_DATAKEYS_MAX
#define _POSIX_DATAKEYS_MAX     8
#endif

#ifndef _POSIX_TIMER_MAX
#define _POSIX_TIMER_MAX  32
#define TIMER_MAX   _POSIX_TIMER_MAX+2
#endif

#endif /*!_pthread_limits_h*/
