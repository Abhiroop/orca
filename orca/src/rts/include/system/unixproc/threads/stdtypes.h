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

  @(#)stdtypes.h	1.20 10/1/93

*/

#ifndef _pthread_stdtypes_h
#define _pthread_stdtypes_h

#ifndef	__sys_stdtypes_h
#define	__sys_stdtypes_h

typedef	unsigned int	sigset_t;	/* signal mask - may change */

typedef	unsigned int	speed_t;	/* tty speeds */
typedef	unsigned long	tcflag_t;	/* tty line disc modes */
typedef	unsigned char	cc_t;		/* tty control char */
typedef	int		pid_t;		/* process id */

typedef	unsigned short	mode_t;		/* file mode bits */
typedef	short		nlink_t;	/* links to a file */

typedef	long		clock_t;	/* units=ticks (typically 60/sec) */
typedef	long		time_t;		/* value = secs since epoch */

typedef	int		size_t;		/* ??? */
typedef int		ptrdiff_t;	/* result of subtracting two pointers */

typedef	unsigned short	wchar_t;	/* big enough for biggest char set */

#endif	/* !__sys_stdtypes_h */

#endif /*!_pthread_stdtypes_h*/
