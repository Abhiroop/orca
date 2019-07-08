/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"		/* Provides a system interface */
#include "pan_threads.h"
#include "pan_global.h"
#include "pan_error.h"
#include "pan_prioq.h"

#include <string.h>
#include <signal.h>

#define PAN_MIN_PRIO		1
#define PAN_MAX_PRIO		100

#define DEFAULT_STACKSIZE	0
#define DEFAULT_PRIORITY	PAN_MIN_PRIO

static pan_thread_p main_thread;				/* Added RFHH */
/* static */ ot_queue_t pan_runq;		/* runq for panda threads */
static unsigned pan_thread_counter = 0; /* for thread id's */
static pan_mutex_p thread_counter_lock;

static pan_mutex_p join_lock;
static pan_cond_p join_cond;
static pan_mutex_p exit_lock;
static pan_thread_p exit_thread;


static void
pan_exit_cleanup_handler (void *ptr)
{
    pan_thread_p thread = (pan_thread_p) ptr;

    /* Don't execute any code that (might) grab a lock because ot can go
     * into deadlock in the case of a blocking cleanup function.
     */
    thread->flags |= PAN_THREAD_EXITED;
}

/* 
  Should be called before any other routine in this module. 
  Initialises some data structures.
*/
void pan_sys_thread_start()
{
    /* We want a priority queue, so override default (FIFO) policy.
     */
    otm_install_queue( PAN_PRIOQ_LINKSIZE, PAN_PRIOQ_IMPSIZE,
                       (otm_qinitf_t *) pan_prioq_init,
                       (otm_qgetf_t *) pan_prioq_get,
                       (otm_qputf_t *) pan_prioq_put);
    ot_queue_init (&pan_runq);
    ot_begin_mt (&pan_runq);	/* begin multithreaded execution */
    thread_counter_lock = pan_mutex_create ();
    exit_lock = pan_mutex_create ();
    join_lock = pan_mutex_create ();
    join_cond = pan_cond_create (join_lock);

    main_thread = pan_malloc(sizeof(struct pan_thread));
    assert(main_thread);

    ot_thread_setspecific( ot_current_thread(), main_thread, 
			   pan_exit_cleanup_handler);
    pan_thread_setprio( DEFAULT_PRIORITY);
}


/* 
  Should be called to end thread usage
*/
void
pan_sys_thread_end(void)
{
    pan_mutex_lock( exit_lock);
    if (exit_thread) {
	pan_thread_clear( exit_thread);
    }
    pan_mutex_unlock( exit_lock);

    ot_end_mt();		/* stop multithreaded execution */
    pan_mutex_clear (thread_counter_lock);
    pan_cond_clear (join_cond);
    pan_mutex_clear (join_lock);
    pan_mutex_clear (exit_lock);

    pan_free(main_thread);
}

						/* Added RFHH */
static void *
thread_wrapper(void *arg)
{
    pan_thread_p thread = arg;

    ot_thread_setspecific( &thread->tcb, thread, pan_exit_cleanup_handler);
    thread->func(thread->arg);

    return NULL;
}


pan_thread_p
pan_thread_create(void (*func)(void *arg), void *arg, long stacksize, 
		  int priority, int detach)
{
    pan_thread_p thread;
    unsigned this_counter;
    size_t s;

    thread = pan_malloc(sizeof(struct pan_thread));
    thread->flags = 0x0;

    if (stacksize == 0L){
	s = DEFAULT_STACKSIZE;
    }else{
	s = (size_t) stacksize;
    }

    if (priority == 0){
	priority = DEFAULT_PRIORITY;
    }

    /* ignoring priority for now; assuming all threads are equal */

    if (detach)
	thread->flags |= PAN_THREAD_DETACHED;

    thread->func = func;			/* Changed RFHH */
    thread->arg  = arg;
    thread->prio = priority;

    pan_mutex_lock (thread_counter_lock);
    this_counter = pan_thread_counter++;
    pan_mutex_unlock (thread_counter_lock);

    ot_thread_init (&thread->tcb, thread_wrapper, thread, 
		    this_counter, &pan_runq);

    return thread;
}

void
pan_thread_clear(pan_thread_p thread)
{
    while (!(thread->flags & PAN_THREAD_EXITED)) {
        ot_thread_yield ();
    }
    pan_free(thread);
}


void
pan_thread_exit()
{
    pan_thread_p thread = pan_thread_self();

    if (thread->flags & PAN_THREAD_DETACHED) {
        pan_mutex_lock( exit_lock);
        if (exit_thread) {
	    pan_thread_clear( exit_thread);
        }
        exit_thread = thread;
        pan_mutex_unlock( exit_lock);
    } else {
        pan_mutex_lock( join_lock);
        thread->flags |= PAN_THREAD_DEAD;
        pan_cond_broadcast( join_cond);
        pan_mutex_unlock( join_lock);
    }
    ot_thread_exit ();
}

void
pan_thread_join(pan_thread_p thread)
{

    pan_mutex_lock( join_lock);
    while (!(thread->flags & PAN_THREAD_DEAD)) {
	pan_cond_wait( join_cond);
    }
    pan_mutex_unlock( join_lock);
}

void
pan_thread_yield(void)
{
    ot_thread_yield ();
}

pan_thread_p
pan_thread_self(void)
{
    return (pan_thread_p) ot_thread_getspecific( ot_current_thread());
}

/* pan_sys_thread_upgrade:
 *                 Upgrade a Solaris thread to a Panda thread
 */
void 
pan_sys_thread_upgrade(pan_thread_p thread)
{
    thread->func = NULL;
    thread->arg = NULL;
}


int pan_thread_getprio(void)
{
    return pan_thread_self()->prio;
}

int pan_thread_setprio(int prio)
{
    pan_thread_p me = pan_thread_self();
    int old = me->prio;

    me->prio = prio;
    return old;
}

int pan_thread_minprio(void)
{
    return PAN_MIN_PRIO;
}

int pan_thread_maxprio(void)
{
    return PAN_MAX_PRIO-1;	/* Keep highest prio for system threads */
}
