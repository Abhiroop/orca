/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"		/* Provides a system interface */
#include "pan_threads.h"
#include "pan_global.h"
#include "pan_error.h"

#include <string.h>
#include <signal.h>

#define DEFAULT_STACKSIZE 0
#define DEFAULT_PRIORITY  0

#define SIGCANCEL  SIGUSR1

/*static  thread_t sleep_thread;*/


static pan_key_p thread_self_key;				/* Added RFHH */
static pan_thread_p main_thread;				/* Added RFHH */


static void
handler(int sig)
{
    thr_exit(NULL);
}


/* 
  Should be called before any other routine in this module. 
  Initialises some data structures.
*/
void pan_sys_thread_start(void)
{
    signal(SIGCANCEL, handler);

    thread_self_key = pan_key_create();				/* Added RFHH */

    main_thread = pan_malloc(sizeof(struct pan_thread));
    assert(main_thread);

    pan_key_setspecific(thread_self_key, main_thread);		/* Added RFHH */
}


/* 
  Should be called to end thread usage
*/
void
pan_sys_thread_end(void)
{
    pan_key_clear(thread_self_key);				/* Added RFHH */
    pan_free(main_thread);

    /*$if ((errno = thr_kill(sleep_thread, SIGCANCEL)) != 0) {
        pan_panic("thread kill");
    }$*/
}

						/* Added RFHH */
static void *
thread_wrapper(void *arg)
{
    pan_thread_p thread = arg;

    pan_key_setspecific(thread_self_key, thread);
    thread->func(thread->arg);

    return NULL;
}


pan_thread_p
pan_thread_create(void (*func)(void *arg), void *arg, long stacksize, 
		  int priority, int detach)
{
    pan_thread_p thread;
    long flags;
    size_t s;

    thread = pan_malloc(sizeof(struct pan_thread));

    if (stacksize == 0L){
	s = DEFAULT_STACKSIZE;
    }else{
	s = (size_t) stacksize;
    }

    if (priority == 0){
	priority = DEFAULT_PRIORITY;
    }

    flags = THR_SUSPENDED;
    if (detach) flags |= THR_DETACHED;

    thread->func = func;			/* Changed RFHH */
    thread->arg  = arg;

    if ((errno = thr_create(NULL, s, thread_wrapper, thread, flags,
			    &thread->thread)) != 0){
	pan_panic("thread create");
    }

    if ((errno = thr_setprio(thread->thread, priority)) != 0){
	pan_panic("set priority");
    }

    if ((errno = thr_continue(thread->thread)) != 0) {
        pan_panic("thread continue");
    }

    return thread;
}

void
pan_thread_clear(pan_thread_p thread)
{
    pan_free(thread);
}


void
pan_thread_exit()
{
    thr_exit(NULL);
}


void
pan_thread_join(pan_thread_p thread)
{
    void *result;
    
#ifdef DEBUG 
    printf("try join %lx\n", thread->thread);
#endif 
    if ((errno = thr_join(thread->thread, NULL, &result)) != 0){
	pan_panic("thread join");
    }
#ifdef DEBUG
    printf("joined %lx\n", thread->thread);
#endif
}


/*$void
pan_thread_detach(pan_thread_p thread)
{
    if ((errno = thr_create(NULL, 0, reaper, thread->thread,
        THR_DETACHED, (thread_t *) 0)) != 0) { 
        pan_panic("thread detach");
    }
}$*/


/*$void
pan_thread_cancel(pan_thread_p thread)
{
    if ((errno = thr_kill(thread->thread, SIGCANCEL)) != 0) {
        pan_panic("thread kill");
    }
}$*/


void
pan_thread_yield(void)
{
    thr_yield();
}


pan_thread_p
pan_thread_self(void)
{
    return pan_key_getspecific(thread_self_key);
}


int
pan_thread_getprio(void)
{
    int r;

    if ((errno = thr_getprio(thr_self(), &r)) != 0){
	pan_panic("get priority");
    }
    return r;
}


int
pan_thread_setprio(int priority)
{
    int old;

    if ((errno = thr_getprio(thr_self(), &old)) != 0){
	pan_panic("Getting old priority");
    }

    if ((errno = thr_setprio(thr_self(), priority)) != 0){
	pan_panic("setting priority");
    }

    return old;
}
      
int
pan_thread_minprio(void)
{
    return 0;
}

int
pan_thread_maxprio(void)
{
    return 100;			/* Should be sufficient */
}

/* pan_sys_thread_upgrade:
 *                 Upgrade a Solaris thread to a Panda thread
 */
void 
pan_sys_thread_upgrade(pan_thread_p thread)
{
    thread->func = NULL;
    thread->arg = NULL;
    
    pan_key_setspecific(thread_self_key, thread);
}
