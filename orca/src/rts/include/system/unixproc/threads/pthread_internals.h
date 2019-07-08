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

  @(#)pthread_internals.h	1.20 10/1/93

*/

#ifndef _pthread_pthread_internals_h
#define _pthread_pthread_internals_h

/*
 * Pthreads interface internals
 */

#include "signal_internals.h"
#include "pthread.h"

#ifndef SRP
#ifdef _POSIX_THREADS_PRIO_PROTECT
 ERROR: undefine _POSIX_THREADS_PRIO_PROTECT in unistd.h when SRP is undefined!
#endif 
#else /* SRP */
#ifndef _POSIX_THREADS_PRIO_PROTECT
 ERROR: define _POSIX_THREADS_PRIO_PROTECT in unistd.h when SRP is defined!
#endif
#endif

/* Other Program Specific Constants */
#define MAX_PRIORITY        101
#define MIN_PRIORITY        0
#define DEFAULT_PRIORITY    MIN_PRIORITY
#define DEFAULT_STACKSIZE   32768	/* changed from 10240  (CJ) */
#define MAX_STACKSIZE       2097152
#define PTHREAD_BODY_OFFSET 200

#ifdef STACK_CHECK
/*
 * page alignment
 */
#define PA(X) ((((int)X)+((int)pthread_page_size-1)) & \
				~((int)pthread_page_size-1))
#endif

#define MAX(x, y) ((x > y)? x : y)
/*
 * timer queue
 */
#ifdef DEF_RR
typedef struct timer_ent {
        struct timeval tp;                     /* wake-up time                */
        pthread_t thread;                      /* thread                      */
        int mode;                              /* mode of timer (ABS/REL/RR)  */
        struct timer_ent *next[TIMER_QUEUE+1]; /* next request in the queue   */
} *timer_ent_t;
#else
typedef pthread_t timer_ent_t;
#endif
 
#ifdef DEF_RR
typedef struct pthread_timer_q_s {
	struct timer_ent *head;
	struct timer_ent *tail;
} pthread_timer_q;
#else
typedef struct pthread_queue pthread_timer_q;
#endif
typedef pthread_timer_q *pthread_timer_q_t;

#define NO_QUEUE          ((pthread_queue_t) NULL)

#define NO_TIMER_QUEUE    ((pthread_timer_q_t) NULL)

#define NO_TIMER	  ((timer_ent_t) NULL)

#define NO_QUEUE_INDEX    0

#define	NO_QUEUE_ITEM	  ((struct pthread *) NULL)

#define	QUEUE_INITIALIZER { NO_QUEUE_ITEM, NO_QUEUE_ITEM }

#define	pthread_queue_init(q) \
	((q)->head = (q)->tail = NO_QUEUE_ITEM)

#define	pthread_timer_queue_init(q) \
	((q)->head = (q)->tail = NO_TIMER)

#ifdef _POSIX_THREADS_PRIO_PROTECT
static pthread_mutexattr_t pthread_mutexattr_default = {1,DEFAULT_PRIORITY,NO_PRIO_INHERIT,};
#else
static pthread_mutexattr_t pthread_mutexattr_default = {1,};
#endif

#ifdef _POSIX_THREADS_PRIO_PROTECT 
#ifdef SRP 
#define MUTEX_WAIT -2
#define NO_PRIO -1
#endif
#endif

#define	MUTEX_INITIALIZER	{ 0, QUEUE_INITIALIZER, 0, 1 }
#define NO_MUTEX      ((pthread_mutex_t *)0)
#define MUTEX_VALID 0x1

static pthread_condattr_t pthread_condattr_default={1,};

#define	CONDITION_INITIALIZER	{ QUEUE_INITIALIZER, 1, 0 }
#define NO_COND       ((pthread_cond_t *) 0)
#define COND_VALID 0x1

/*
 * SIGKILL, SIGSTOP cannot be masked; therefore reused as masks
 */
#define TIMER_SIG       SIGKILL
#define IO_MASK         sigmask(SIGSTOP)

#define	NO_PTHREAD	((pthread_t) 0)

#ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
#ifdef DEF_RR
static pthread_attr_t pthread_attr_default={1,DEFAULT_STACKSIZE,PTHREAD_SCOPE_LOCAL,PTHREAD_DEFAULT_SCHED,0,SCHED_RR,DEFAULT_PRIORITY,};
#else
static pthread_attr_t pthread_attr_default={1,DEFAULT_STACKSIZE,PTHREAD_SCOPE_LOCAL,PTHREAD_DEFAULT_SCHED,0,SCHED_FIFO,DEFAULT_PRIORITY,};
#endif
#else
#ifdef DEF_RR
static pthread_attr_t pthread_attr_default={1,DEFAULT_STACKSIZE,PTHREAD_SCOPE_LOCAL,PTHREAD_DEFAULT_SCHED,0,SCHED_RR,};
#else
static pthread_attr_t pthread_attr_default={1,DEFAULT_STACKSIZE,PTHREAD_SCOPE_LOCAL,PTHREAD_DEFAULT_SCHED,0,SCHED_FIFO,};
#endif
#endif

#define NO_ATTRIBUTE ((pthread_attr_t *)0)

struct cleanup {
  void (*func)();
  any_t arg;
  struct cleanup *next;
};

typedef struct kernel {
  pthread_t k_pthread_self;            /* thread that is currently running   */
  int k_is_in_kernel;                  /* flag to test if in kernel          */
  int k_state_change;                  /* dispatcher state (run q/signals)   */
  sigset_t k_new_signals;              /* bit set of new signals to handle   */
  sigset_t k_pending_signals;          /* bit set of pending signals         */
  int k_S_ENABLE;                      /* mask to enable all signals         */
  int k_S_DISABLE;                     /* mask to disable all signals        */
  char *k_process_stack_base;          /* stack base of process              */
  struct pthread_queue k_ready;        /* ready queue                        */
  struct pthread_queue k_all;          /* queue of all threads               */
  sigset_t k_handlerset;               /* set of signals with user handler   */
  FILE *k_stderr;                      /* file descriptor for stderr         */
  char *k_set_warning;                 /* pointer to set warning message     */
  char *k_clear_warning;               /* pointer to clear warning message   */
} kernel_t;

#ifdef PTHREAD_KERNEL
kernel_t pthread_kern;
#else
extern kernel_t pthread_kern;
#endif

/* Internal Functions */

/*
 * changed for speed-up and interface purposes -
 * pthread_self() is now a function, but mac_pthread_self() is used internally
 * #define pthread_self()  (pthread_kern.k_pthread_self == 0? \
 *                          NO_PTHREAD : pthread_kern.k_pthread_self)
 */
#define mac_pthread_self() pthread_kern.k_pthread_self
#define state_change       pthread_kern.k_state_change
#define is_in_kernel       pthread_kern.k_is_in_kernel
#define new_signals        pthread_kern.k_new_signals
#define pending_signals    pthread_kern.k_pending_signals
#define S_ENABLE           pthread_kern.k_S_ENABLE
#define S_DISABLE          pthread_kern.k_S_DISABLE
#define process_stack_base pthread_kern.k_process_stack_base
#define ready              pthread_kern.k_ready
#define all                pthread_kern.k_all
#define handlerset         pthread_kern.k_handlerset
#define set_warning        pthread_kern.k_set_warning
#define clear_warning      pthread_kern.k_clear_warning

#ifdef DEBUG
#define SET_KERNEL_FLAG \
  MACRO_BEGIN \
    if (is_in_kernel) \
      fprintf(stderr, set_warning); \
    else \
      is_in_kernel = TRUE; \
  MACRO_END
#else
#define SET_KERNEL_FLAG is_in_kernel = TRUE
#endif

#ifdef C_CONTEXT_SWITCH

#define SHARED_CLEAR_KERNEL_FLAG \
  MACRO_BEGIN \
    is_in_kernel = FALSE; \
    if (state_change) { \
      is_in_kernel = TRUE; \
      if ((new_signals || mac_pthread_self() != ready.head) && \
          !sigsetjmp(&mac_pthread_self()->context, FALSE)) \
        pthread_sched(); \
      state_change = FALSE; \
      is_in_kernel = FALSE; \
      while (new_signals) { \
        is_in_kernel = TRUE; \
        pthread_sched_new_signals(mac_pthread_self(), TRUE); \
        if (!sigsetjmp(&mac_pthread_self()->context, FALSE)) \
          pthread_sched(); \
        state_change = FALSE; \
        is_in_kernel = FALSE; \
      } \
    } \
  MACRO_END

#else /* !C_CONTEXT_SWITCH */
#ifdef NO_INLINE

#define CLEAR_KERNEL_FLAG pthread_sched()

#else /* !NO_INLINE */

#define SHARED_CLEAR_KERNEL_FLAG \
  MACRO_BEGIN \
    is_in_kernel = FALSE; \
    if (state_change) \
      pthread_sched(); \
  MACRO_END
#endif /* NO_INLINE */

#endif /* C_CONTEXT_SWITCH */


#ifdef RR_SWITCH
#define CLEAR_KERNEL_FLAG \
  MACRO_BEGIN \
    if ((mac_pthread_self()->queue == &ready) && (ready.head != ready.tail)) { \
      pthread_q_deq(&ready,mac_pthread_self(),PRIMARY_QUEUE); \
      pthread_q_enq_tail(&ready); \
    } \
    SHARED_CLEAR_KERNEL_FLAG; \
  MACRO_END

#elif RAND_SWITCH
#define CLEAR_KERNEL_FLAG \
  MACRO_BEGIN \
    if ((mac_pthread_self()->queue == &ready) && (ready.head != ready.tail) \
        && ((int)random()&01)) { \
      pthread_q_exchange_rand(&ready); \
    } \
    SHARED_CLEAR_KERNEL_FLAG; \
  MACRO_END

#elif DEBUG
#define CLEAR_KERNEL_FLAG \
  MACRO_BEGIN \
    if (!is_in_kernel) \
      fprintf(stderr, clear_warning); \
    SHARED_CLEAR_KERNEL_FLAG; \
  MACRO_END

#else
#define CLEAR_KERNEL_FLAG SHARED_CLEAR_KERNEL_FLAG
#endif

#ifdef C_CONTEXT_SWITCH
#define SIG_CLEAR_KERNEL_FLAG(b) \
  MACRO_BEGIN \
    if(!sigsetjmp(&mac_pthread_self()->context, FALSE)) \
      pthread_handle_pending_signals_wrapper(); \
    state_change = FALSE; \
    is_in_kernel = FALSE; \
    while (new_signals) { \
      is_in_kernel = TRUE; \
      pthread_sched_new_signals(mac_pthread_self(), TRUE); \
      if (!sigsetjmp(&mac_pthread_self()->context, FALSE)) \
        pthread_sched(); \
      state_change = FALSE; \
      is_in_kernel = FALSE; \
    } \
  MACRO_END
#else /* !C_CONTEXT_SWITCH */
#define SIG_CLEAR_KERNEL_FLAG(b) pthread_handle_pending_signals_wrapper(b)
#endif /* C_CONTEXT_SWITCH */

#ifdef SIM_KERNEL
#define SIM_SYSCALL(cond) if (cond) sigblock(0)
#else
#define SIM_SYSCALL(cond)
#endif

/*
 * Errno is mapped on process' _errno and swapped upon context switch
 */
#define set_errno(e)    (errno = e)
#define get_errno()     (errno)
/*
 * #define set_errno(e)    (mac_pthread_self()->context.errno = e)
 * #define get_errno()     (mac_pthread_self()->context.errno)
 */

#ifndef	MACRO_BEGIN

#define	MACRO_BEGIN	do {

#ifndef	lint
#define	MACRO_END	} while (0)
#else /*	lint */
extern int _NEVER_;
#define	MACRO_END	} while (_NEVER_)
#endif /*	lint */

#endif /*	not MACRO_BEGIN */

/*
 * Debugging support.
 */

#ifdef	DEBUG

#ifndef	ASSERT
/*
 * Assertion macro, similar to <assert.h>
 */
#define	ASSERT(p) \
  MACRO_BEGIN \
    if (!(p)) { \
      printf("File %s, line %d: assertion p failed.\n", __FILE__, __LINE__); \
      abort(); \
    } \
  MACRO_END

#endif /*	ASSERT */

#else /*	DEBUG */

#ifndef	ASSERT
#define	ASSERT(p)
#endif /*	ASSERT */

#endif /*	DEBUG */

#endif /*!_pthread_pthread_internals_h*/
