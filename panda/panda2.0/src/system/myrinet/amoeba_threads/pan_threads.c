#include "fm.h"

#include "amoeba.h"
#include "thread.h"

#include "pan_sys_msg.h"		/* Provides a system interface */
#include "pan_threads.h"
#include "pan_global.h"
#include "pan_error.h"

#define DEFAULT_STACKSIZE 65536
#define DEFAULT_PRIORITY      1

static pan_thread_p main_thread;

static long  thread_maxprio;
static int   thread_key;


void
pan_poll(void)
{
#ifndef FM_NO_INTERRUPTS
    FM_disable_intr();
#endif
    FM_extract();
    /* FM_enable_intr(); */
}

void
pan_sys_thread_option( char *str)
{
#ifdef THREAD_STATISTICS
     if (strcmp( str, "no_intr") == 0) {
	pan_thread_intr = 0;
     } else if (strcmp( str, "no_poll") == 0) {
	pan_thread_poll = 0;
     } else if (strcmp( str, "statistics") == 0) {
	pan_thread_statistics = 1;
     } else if (strcmp( str, "verbose") == 0) {
	pan_thread_verbose = 1;
     } else
#endif
	pan_panic( "thread module: unknown option %s", str);
}


static void
thread_wrapper(char *param, int psize)
{
    long old;
    pan_thread_p me = (pan_thread_p)param, *me_ptr;
    
    /*
     * Store the pointer to the thread structure (me) in glocal data so
     * that we can always find it again.
     */
    me_ptr = (pan_thread_p *)thread_alloc(&thread_key, sizeof(pan_thread_p));
    if (me_ptr) {
	*me_ptr = me;
    } else {
        pan_panic("thread_wrapper: thread_alloc failed\n");
    }
	
#ifndef QPT
    thread_set_priority(me->prio, &old);
#endif

    sema_up(&me->prio_set);
    sema_down(&me->run);
    
    (*me->func)(me->arg);
    
    /*
     * Should never get here!
     */
    pan_panic("thread_wrapper: thread never called pan_thread_exit()\n");
}


void
pan_sys_thread_start(void)
{
    pan_thread_p *main_thread_ptr;

    /*
     * pan_sys_thread_start() should only be called from the main
     * thread. Since this thread is not created by pan_thread_create(),
     * we explicitly build a thread structure for it, so that it can
     * safely call pan_thread_self().
     */
    main_thread = pan_malloc(sizeof(struct pan_thread));
    main_thread->prio = 0;
    sema_init(&main_thread->join, 0);
    sema_init(&main_thread->kill, 0);
    sema_init(&main_thread->prio_set, 0);
    sema_init(&main_thread->run, 0);
    main_thread->func = 0;
    main_thread->arg = 0;
    main_thread->detached = 1;

    main_thread_ptr = (pan_thread_p *)thread_alloc(&thread_key,
						   sizeof(pan_thread_p));
    if (main_thread_ptr) {
	*main_thread_ptr = main_thread;
    } else {
        pan_panic("pan_sys_thread_start: thread_alloc failed\n");
    }

#ifndef QPT
    thread_enable_preemption();
#endif
    thread_get_max_priority(&thread_maxprio);
    pan_thread_setprio(pan_thread_minprio());
}


void
pan_sys_thread_end(void)
{
    pan_free(main_thread);
    main_thread = 0;
}


pan_thread_p
pan_thread_create(void (*func)(void *arg), void *arg, long stacksize, 
		  int prio, int detach)
{
    pan_thread_p thread;
    int s;

    /*
     * Create and initialise the Panda thread structure.  Put the
     * thread structure in glocal data; this way pan_thread_exit is
     * able to find the thread structure when it is needed.  
     */
					/* Bug patch:
					 * thread is freed from the "system
					 * call" thread_exit, with a call by
					 * _free_, not _ pan_free _.
					 * Therefore, we here call _ malloc _,
					 * not _ pan_malloc _.
					 *			RFHH.
					 */
    thread = malloc(sizeof(struct pan_thread));
    
    if (0 <= prio && prio <= thread_maxprio) {
	thread->prio = prio;
    } else {
	pan_panic("pan_thread_create: illegal priority %d\n", prio);
    }

    sema_init(&thread->join, 0);
    sema_init(&thread->kill, 0);
    sema_init(&thread->prio_set, 0);
    sema_init(&thread->run, 0);
    
    thread->func     = func;
    thread->arg      = arg;
    thread->detached = detach;


    /*
     * The new thread may have a higher priority than the running
     * thread. In that case, it must be run immediately. Unfortunately,
     * on Amoeba, a thread's priority can only be set by the thread
     * itself. So we create the thread and give it the opportunity to set
     * its priority; this allows the scheduler to run the thread.
     */
    s = (stacksize == 0 ? DEFAULT_STACKSIZE : stacksize);
    if (thread_newthread(thread_wrapper, s,
			 (char *)thread, sizeof(struct pan_thread)) == 0)
    {
	pan_panic("pan_thread_create: thread_newthread failed\n");
    }
    sema_down(&thread->prio_set);    /* New thread must set its prio! */
    sema_up(&thread->run);           /* Now it may run */

    return thread;
}


void
pan_thread_clear(pan_thread_p thread)
{
    /*
     * The thread structure has already been deallocated by pan_thread_exit().
     */
}


void
pan_thread_exit()
{
    pan_thread_p me, *me_ptr;

    me_ptr = (pan_thread_p *)thread_alloc(&thread_key, sizeof(pan_thread_p));
    if (me_ptr) {
	me = *me_ptr;
    } else {
	pan_panic("pan_thread_exit: thread_alloc failed\n");
    }

    if (!me->detached) {
	sema_up(&me->join);
	sema_down(&me->kill);
    }
    thread_exit();
}


void
pan_thread_join(pan_thread_p thread)
{
    if (thread->detached) {
	pan_panic("pan_thread_join: cannot join a detached thread\n");
    }
    sema_down(&thread->join);	/* wait for dying thread to arrive */
    sema_up(&thread->kill);	/* tell it to die */
}


void
pan_thread_yield(void)
{
    threadswitch();
}


pan_thread_p
pan_thread_self(void)
{
    pan_thread_p *me_ptr;

    me_ptr = (pan_thread_p *)thread_alloc(&thread_key, sizeof(pan_thread_p));
    if (!me_ptr) {
	pan_panic("pan_thread_self: thread_alloc failed\n");
    }
    return *me_ptr;
}


int
pan_thread_getprio(void)
{
    pan_thread_p *me_ptr;

    me_ptr = (pan_thread_p *)thread_alloc(&thread_key, sizeof(pan_thread_p));
    if (!me_ptr) {
	pan_panic("pan_thread_getprio: thread_alloc failed\n");
    }
    return (*me_ptr)->prio;
}


int
pan_thread_setprio(int priority)
{
    long oldprio;
    pan_thread_p me, *me_ptr;

    me_ptr = (pan_thread_p *)thread_alloc(&thread_key, sizeof(pan_thread_p));
    if (me_ptr) {
	me = *me_ptr;
    } else {
	pan_panic("pan_thread_getprio: thread_alloc failed\n");
    }

    if (priority < 0 || priority > thread_maxprio) {
	pan_panic("pan_thread_setprio: illegal priority %d\n", priority);
    }

#ifndef QPT
    thread_set_priority(priority, &oldprio);
#else
    oldprio = me->prio;
#endif
    me->prio = priority;

    return (int)oldprio;
}

      
int
pan_thread_minprio(void)
{
    return DEFAULT_PRIORITY;
}


int
pan_thread_maxprio(void)
{
    return MIN(100, thread_maxprio);       /* Should be sufficient */
}
