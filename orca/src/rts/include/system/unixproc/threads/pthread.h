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

  @(#)pthread.h	1.20 10/1/93

*/

#ifndef _pthread_pthread_h
#define _pthread_pthread_h

#define SRP

/*
 * Pthreads interface definition
 */

#ifndef	_POSIX_THREADS

#include "unistd.h"
#include "signal.h"
#include <stdio.h>
#include <setjmp.h>
#include "limits.h"
#include <sys/time.h>
#include "errno.h"
#include "pthread_asm.h"
#include "setjmp.h"

#define thread_t Thread_t
#include <sys/asynch.h>
#undef thread_t
#ifdef MALLOC
#include <malloc.h>
#endif /* MALLOC */

/* contentionscope attribute values */
#define PTHREAD_SCOPE_GLOBAL 0
#define PTHREAD_SCOPE_LOCAL  1

/* delivery modes */
#define PTHREAD_PROCSYNC     0
#define PTHREAD_PROCASYNC    1

/* inheritsched attribute values */
#define PTHREAD_INHERIT_SCHED 0
#define PTHREAD_DEFAULT_SCHED 1

/* Allow variable stack sizes */
#ifndef _POSIX_THREAD_ATTR_STACKSIZE
#define _POSIX_THREAD_ATTR_STACKSIZE
#endif

#if __STDC__ || defined(__cplusplus) || defined(c_plusplus)
#include <stdlib.h>
#ifndef	_C_PROTOTYPE
#define	_C_PROTOTYPE(arglist)	arglist
#define _C_CONST		const
#endif /*	not _C_PROTOTYPE */

typedef void *any_t;

#else /*	! (__STDC__ || defined(__cplusplus) || defined(c_plusplus)) */

#ifndef	_C_PROTOTYPE
#define	_C_PROTOTYPE(arglist)	()
#define _C_CONST
#endif /*	not _C_PROTOTYPE */

typedef char *any_t;

#endif /*	! (__STDC__ || defined(__cplusplus) || defined(c_plusplus)) */

#ifndef	TRUE
#define	TRUE	1
#define	FALSE	0
#endif

#ifndef __sys_stdtypes_h
typedef	long	size_t;
#endif

/*
 * Mutex objects.
 */

typedef int pthread_protocol_t;

typedef struct pthread_queue {
          struct pthread *head;
          struct pthread *tail;
} *pthread_queue_t;

typedef struct mutex {
	struct pthread_queue queue;
	char lock;
        struct pthread *owner;
        int flags;
#ifdef _POSIX_THREADS_PRIO_PROTECT
        int prio_ceiling;
        pthread_protocol_t protocol;
	int prev_max_ceiling_prio;
#endif
#endif
} pthread_mutex_t;

typedef struct pthread_mutexattr {
        int flags;
#ifdef _POSIX_THREADS_PRIO_PROTECT
        int prio_ceiling;
        pthread_protocol_t protocol;
#endif
} pthread_mutexattr_t;

/******************************/
/* Mutex Attributes Functions */
/*       Mutex Functions      */
/******************************/

extern int pthread_mutex_lock     _C_PROTOTYPE((pthread_mutex_t *__mutex));
extern int pthread_mutex_trylock  _C_PROTOTYPE((pthread_mutex_t *__mutex));
extern int pthread_mutex_unlock   _C_PROTOTYPE((pthread_mutex_t *__mutex));
extern int pthread_mutex_init     _C_PROTOTYPE((pthread_mutex_t *__mutex,
						pthread_mutexattr_t *__attr));
extern int pthread_mutex_destroy  _C_PROTOTYPE((pthread_mutex_t *__mutex));
extern int pthread_mutex_setprio_ceiling
                                  _C_PROTOTYPE((pthread_mutex_t *__mutex,
						int __prio_ceiling));
extern int pthread_mutex_getprio_ceiling
                                  _C_PROTOTYPE((pthread_mutex_t __mutex));
extern int pthread_mutexattr_init _C_PROTOTYPE((pthread_mutexattr_t *__attr));
extern int pthread_mutexattr_destroy
                                  _C_PROTOTYPE((pthread_mutexattr_t *__attr));
extern int pthread_mutexattr_setprotocol
                                  _C_PROTOTYPE((pthread_mutexattr_t *__attr,
						pthread_protocol_t __protocol));
extern pthread_protocol_t pthread_mutexattr_getprotocol 
				  _C_PROTOTYPE((pthread_mutexattr_t __attr));

extern int pthread_mutexattr_setprio_ceiling
                                  _C_PROTOTYPE((pthread_mutexattr_t *__attr,
						int __prio_ceiling)); 
extern int pthread_mutexattr_getprio_ceiling
                                  _C_PROTOTYPE((pthread_mutexattr_t __attr));
extern int pthread_mutexattr_getpshared
                                  _C_PROTOTYPE((pthread_mutexattr_t *__attr,
						int *__pshared));
extern int pthread_mutexattr_setpshared
                                  _C_PROTOTYPE((pthread_mutexattr_t *__attr,
						int __pshared));

/*
 * Once Objects.
 */

typedef	struct {
	short init;
	short exec;
	pthread_mutex_t mutex;
} pthread_once_t;

#define PTHREAD_ONCE_INIT	{FALSE,FALSE,0, };

/*
 * Condition variables.
 */
typedef struct condition {
	struct pthread_queue queue;
        int flags;
        int waiters;
        pthread_mutex_t *mutex;
} pthread_cond_t;

typedef struct condattr {
        int flags;
} pthread_condattr_t;

struct timespec {
  time_t tv_sec;
  long   tv_nsec;
};

/*******************************/
/*      Condition Functions    */
/*Condition Attribute Functions*/
/*******************************/

extern int pthread_cond_destroy     _C_PROTOTYPE((pthread_cond_t *__cond));
extern int pthread_cond_init        _C_PROTOTYPE((pthread_cond_t *__cond,
						  pthread_condattr_t *__attr));
extern int pthread_condattr_init    _C_PROTOTYPE((pthread_condattr_t *__attr));
extern int pthread_condattr_destroy _C_PROTOTYPE((pthread_condattr_t *__attr));
extern int pthread_cond_wait        _C_PROTOTYPE((pthread_cond_t *__cond,
						  pthread_mutex_t *__mutex));
extern int pthread_cond_timedwait   _C_PROTOTYPE((pthread_cond_t *__cond,
						  pthread_mutex_t *__mutex,
						  struct timespec *__timeout));
extern int pthread_cond_signal      _C_PROTOTYPE((pthread_cond_t *__cond));
extern int pthread_cond_broadcast   _C_PROTOTYPE((pthread_cond_t *__cond));
extern int pthread_condattr_getpshared
                                    _C_PROTOTYPE((pthread_condattr_t *__attr,
						  int *__pshared));
extern int pthread_condattr_setpshared
                                    _C_PROTOTYPE((pthread_condattr_t *__attr,
						  int __pshared));

/*
 * Threads.
 */

typedef any_t (*pthread_func_t) _C_PROTOTYPE((any_t __arg));

typedef struct pthread_attr {
  int flags;
#ifdef _POSIX_THREAD_ATTR_STACKSIZE
  int stacksize;
#endif
  int contentionscope;
  int inheritsched;
  int detachstate;
  int sched;
#ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
  int prio;                      /* (Active) priority of Thread        */
#endif
} pthread_attr_t;

/*
 * Queue indices
 */
#define PRIMARY_QUEUE 0
#define ALL_QUEUE     1
#define NUM_QUEUES    3

#ifdef DEF_RR
#define TIMER_QUEUE   0
#else
#define TIMER_QUEUE   2
#endif

typedef int pthread_key_t;

typedef struct cleanup *cleanup_t;

typedef struct pthread {
	struct sigjmp_buf context;         /* save area for context switch   */
	char *stack_base;                  /* bottom of run-time stack       */
	int state;                         /* thread state, -> pthread_asm.h */
        struct pthread *next[NUM_QUEUES];  /* links for queues               */
	int num_timers;			   /* number of timers for thread    */
	struct timeval interval;	   /* time left for SCHED_RR 	     */
        struct siginfo sig_info[NNSIG];    /* info for user handlers         */
        struct sigcontext *nscp;           /* UNIX signal context (new sig)  */
        struct sigcontext *scp;            /* UNIX signal context (current)  */
        struct pthread_queue joinq;        /* queue to await termination     */
        pthread_cond_t *cond;              /* cond var. if in cond_wait      */
        pthread_queue_t queue;/* primary queue thread is contained in        */
        sigset_t mask;        /* set of signals thread has masked out        */
        sigset_t pending;     /* set of signals pending on thread            */
        sigset_t sigwaitset;  /* set of signals thread is waiting for        */
        struct timeval tp;    /* wake-up time                                */
	pthread_func_t func;  /* actual function to call upon activation     */
	any_t arg;            /* argument list to above function             */
        any_t result;         /* return value of above function              */
	any_t key[_POSIX_DATAKEYS_MAX];    /* thread specific data           */
        cleanup_t cleanup_top;             /* stack of cleanup handlers      */
        struct pthread_attr attr;          /* attributes                     */
#ifdef _POSIX_THREADS_PRIO_PROTECT
        int base_prio;                     /* Base priority of thread        */
	int max_ceiling_prio; /* Max of ceiling prio among locked mutexes    */
        int new_prio;         /* New Priority                                */
#endif
	struct	aio_result_t	resultp;
} *pthread_t;


/******************************/
/*      Thread Functions      */
/* Thread Attribute Functions */
/******************************/

extern pthread_t pthread_self           _C_PROTOTYPE ((void));
extern void pthread_init                _C_PROTOTYPE ((void));
extern int pthread_create               _C_PROTOTYPE((pthread_t *__thread,
						      pthread_attr_t *__attr,
						      pthread_func_t __func,
						      any_t __arg));
extern int pthread_equal                _C_PROTOTYPE((pthread_t __t1,
						      pthread_t __t2));
extern int pthread_detach               _C_PROTOTYPE((pthread_t *__thread));
extern int pthread_join                 _C_PROTOTYPE((pthread_t __thread,
						      any_t *__status));
extern void pthread_yield               _C_PROTOTYPE((any_t __arg));
extern void pthread_exit                _C_PROTOTYPE((any_t __status));
extern int pthread_attr_init            _C_PROTOTYPE((pthread_attr_t *__attr));
extern int pthread_attr_destroy         _C_PROTOTYPE((pthread_attr_t *__attr));
extern int pthread_getschedattr         _C_PROTOTYPE((pthread_t __thread,
						      pthread_attr_t *__attrs));
extern int pthread_setschedattr         _C_PROTOTYPE((pthread_t __thread,
						      pthread_attr_t __attrs));
#ifdef DRAFT5
extern int pthread_setprio		_C_PROTOTYPE((pthread_t __thread,
						      int __prio));
#endif
extern int pthread_attr_setstacksize    _C_PROTOTYPE((pthread_attr_t *__attr,
						      size_t __stacksize));
extern int pthread_attr_getstacksize    _C_PROTOTYPE((pthread_attr_t *__attr,
						      size_t *__stacksize));
extern int pthread_attr_setscope        _C_PROTOTYPE((pthread_attr_t *__attr,
						      int __contentionscope));
extern int pthread_attr_setinheritsched _C_PROTOTYPE((pthread_attr_t *__attr,
						      int __inherit));
extern int pthread_attr_setsched        _C_PROTOTYPE((pthread_attr_t *__attr,
						      int __scheduler));
extern int pthread_attr_setprio         _C_PROTOTYPE((pthread_attr_t *__attr,
						      int __priority));
extern int pthread_attr_getscope        _C_PROTOTYPE((pthread_attr_t __attr));
extern int pthread_attr_getinheritsched _C_PROTOTYPE((pthread_attr_t *__attr));
extern int pthread_attr_getsched        _C_PROTOTYPE((pthread_attr_t *__attr));
extern int pthread_attr_getprio         _C_PROTOTYPE((pthread_attr_t *__attr));
extern int pthread_key_create           _C_PROTOTYPE((pthread_key_t *__key,
						      void (*__func)
						      (any_t __value)));
extern int pthread_setspecific          _C_PROTOTYPE((pthread_key_t __key,
						      any_t __value));
extern int pthread_getspecific          _C_PROTOTYPE((pthread_key_t __key,
						      any_t *__value));
extern int pthread_cleanup_push         _C_PROTOTYPE((void (*__func)
						      (any_t __value),
						      any_t __arg));
extern int pthread_cleanup_pop          _C_PROTOTYPE((int __execute));
extern int sched_get_priority_max       _C_PROTOTYPE((int __policy));
extern int sched_get_priority_min       _C_PROTOTYPE((int __policy));
extern int pthread_attr_setdetachstate  _C_PROTOTYPE((pthread_attr_t *__attr,
						      int *__detachstate));
extern int pthread_attr_getdetachstate  _C_PROTOTYPE((pthread_attr_t *__attr));
extern int pthread_once                 _C_PROTOTYPE((pthread_once_t *__once_c,
						      void (*__func)
						      (any_t __value)));
/*
 * pthread_lock_stack() is not a part of the standard interface. It is
 * provided to check stack_overflow.
 */
#if STACK_CHECK && SIGNAL_STACK
extern int pthread_lock_stack		_C_PROTOTYPE((pthread_t __p));
#endif

/******************************/
/*      Signal Functions      */
/******************************/

extern int sigwait             _C_PROTOTYPE((sigset_t *__set));
#ifndef __GNUC_MINOR__
extern int sigprocmask         _C_PROTOTYPE((int __how,
					     _C_CONST sigset_t *__set,
					     sigset_t *__oset));
#endif /* from <sys/signal.h> */
extern int sigpending          _C_PROTOTYPE((sigset_t *__set));
extern int sigsuspend          _C_PROTOTYPE((sigset_t *__set));
extern int pause               _C_PROTOTYPE((void));
extern int raise               _C_PROTOTYPE((int __sig));
extern int pthread_kill        _C_PROTOTYPE((pthread_t __thread,
					     int __sig));
extern int pthread_cancel      _C_PROTOTYPE((pthread_t __thread));
extern int pthread_setintr     _C_PROTOTYPE((int __state));
extern int pthread_setintrtype _C_PROTOTYPE((int __type));
extern void pthread_testintr   _C_PROTOTYPE((void));
#ifndef __GNUC_MINOR__
extern int sigaction           _C_PROTOTYPE((int __sig,
					     _C_CONST struct sigaction *__act,
					     struct sigaction *__oact));
#endif /* from <sys/signal.h> */
/* yet to come...
extern unsigned int alarm      _C_PROTOTYPE((unsigned int __seconds));
*/
extern int nanosleep           _C_PROTOTYPE((struct timespec *__rqtp,
					     struct timespec *__rmtp));
extern unsigned int sleep      _C_PROTOTYPE((unsigned int __seconds));
extern int clock_gettime       _C_PROTOTYPE((int __clock_id,
					     struct timespec *__tp));

/******************************/
/*    Low-Level Functions     */
/******************************/

extern int setjmp         _C_PROTOTYPE((jmp_buf __env));
extern void longjmp       _C_PROTOTYPE((jmp_buf __env,
					int __val));
extern int sigsetjmp      _C_PROTOTYPE((sigjmp_buf __env,
					int __savemask));
extern void siglongjmp    _C_PROTOTYPE((sigjmp_buf __env,
					int __val));

/******************************/
/*       I/O Functions        */
/******************************/

/*
extern int read  _C_PROTOTYPE((int __fd,
			       void *__buf,
			       unsigned int __nbytes));
extern int write _C_PROTOTYPE((int __fd,
			       _C_CONST void *__buf,
			       unsigned int __nbytes));
*/

#endif /*!_pthread_pthread_h*/
