/*
 * Acknowledgements:
 * -----------------
 * This code was originally developed by Frans Kaashoek's PDOS group
 * at MIT. It was modified in several ways before it was incorporated
 * in Panda.
 *
 * Implementation notes:
 * ---------------------
 * 1) Scheduling is nonpreemptive. This makes the implementation of
 *    the synchronisation primitives simpler, but it also requires that
 *    the network be polled.
 *
 * 2) Priorities are ignored. Users can specify priorities in the
 *    range [0..100], but these are never taken into account by the
 *    scheduler. The range [0..100] was chosen to ensure that existing
 *    Panda programs continue to work.
 *
 * 3) We removed the STRATA and HIGHPRIO code.
 *
 * 4) We simplified the scheduling code.
 */

#include <stdio.h>
#include<sys/types.h>
#include <stdarg.h>

#include "pan_sys.h"		/* Provides a system interface */
#include "pan_system.h"
#include "pan_threads.h"
#include "pan_global.h"
#include "pan_error.h"
#include "pan_const.h"
#include "pan_sync.h"

#include "pan_trace.h"

#include "amoeba.h"
#include "module/mutex.h"
#include "thread.h"
#include "exception.h"
#include "fault.h" /* From src/h/machdep/arch/<architecture> */
#include "module/signals.h"

#include "fm.h"

/* These prototypes have not made it to the public fm.h yet: */
extern void FM_print_lcp_mcgroup(int);
extern void FM_print_mc_credit(void);
extern void _await_intr(int, int);
extern void FM_enable_intr(void);
extern void FM_disable_intr(void);
extern void FM_set_parameter(int, ...);
extern unsigned int FM_pending(void);


#ifdef THREAD_STATISTICS
#  define STATINC(n)	(++(n))
#else
#  define STATINC(n)
#endif


#ifdef CMOST

#include <sun4/asm_linkage.h>
#include <sun4/frame.h>
#include <cm/cmna.h>

#elif AMOEBA

/*
 * Definition of the sparc stack frame (when it is pushed on the stack).
 * I'm sure its in a header file somewhere...
 */
struct sparc_frame {
	int	fr_local[8];		/* saved locals */
	int	fr_arg[6];		/* saved arguments [0 - 5] */
	struct sparc_frame *fr_savfp;	/* saved frame pointer */
	int	fr_savpc;		/* saved program counter */
	char	*fr_stret;		/* struct return addr */
	int	fr_argd[6];		/* arg dump area */
	int	fr_argx[1];		/* array of args past the sixth */
};

#endif

#ifdef DEBUG
static int my_address;
#endif


/* Use a negative signal since it avoids signal blocking
 * overhead which we can do without.
 */
#define NETWORK_SIGNAL	((signum) -123)
#define TIMER_SIGNAL	((signum) 111)	      /* negative sig is not allowed */

#define TIME_SLICE	100	/* msec */

/*
 *		Global thread variables
 */
pan_thread_t pan_all_thread[MAX_THREAD];	/* all threads */
pan_thread_p pan_cur_thread;	  /* ptr to the current running thread */
pan_thread_p pan_thread_run_next;   /* used to signal end of idle loop */

#define pan_idle_thread (&pan_all_thread[1])     /* used by poll in idle loop */

static pan_thread_p free_threads;     /* free thread list */

static long thread_maxprio = 100;

static pan_thread_p pan_handler_thread;  /* polls network on interrupt */
static int network_poll;	         /* signals handler_thread to stop */
#ifdef TIME_SLICING
static mutex timer_lock;	         /* signals timer_thread to stop */
#endif

volatile int pan_thread_sched_busy,      /* protects critical scheduling code */
             pan_thread_queued_interrupt;/* records delayed interrupts */
static volatile int block_interrupts = 0;

#ifdef THREAD_STATISTICS
static int thread_switches = 0;
static int timer_interrupts = 0;
static int network_interrupts = 0;
static int delayed_interrupts = 0;
static int polls = 0;
#endif

/* assembly routine */
extern void pan_thread_switch(pan_thread_p cur, pan_thread_p next);

static void thread_body(void);

static void pan_do_poll(void);

static void do_switch( pan_thread_p t);

/* 
 * 			Thread code
 */

static INLINE pan_thread_p idle_loop( pan_thread_p cur)
{
        /* Performance hack: disable network interrupts when running idle.
         * Abuse block_interrupts variable to avoid re-enabeling interrupts by
         * pan_do_poll().
         */
        FM_disable_intr();
        block_interrupts++;
	assert( block_interrupts == 1);

    	cur->t_state |= T_BLOCKED;  /* Idle thread blocks current stack */
    	pan_cur_thread = pan_idle_thread;
    	pan_idle_thread->t_state |= (T_RUNNABLE | T_RUNNING);

#ifdef TRACING
        {   static int new = 1;

    	    if (new) {
    	        new = 0;
    	        trc_new_thread(0, "idle_thread");
	    }
	}
#endif
        sched_unlock(0);	/* upcalls outside critical section */
	pan_thread_run_next = 0;
	do {
    	    /* poll for a message from the network; inline pan_do_poll()
	     * for efficiency (less function calls => less traps)
	     */
            STATINC( polls);
            FM_extract();

	    if (cur->t_state & T_RUNNABLE) {
		/* The blocked thread has been signalled without calling
		 * pan_thread_prio_schedule(), so set pan_run_next now.
		 */
		pan_thread_run_next = cur;
		break;
	    }
	}
	while ( pan_thread_run_next == 0);
        sched_lock();

        pan_idle_thread->t_state &= (~T_RUNNING) & (~T_RUNNABLE);
    	pan_cur_thread = cur;
    	assert( cur->t_state & T_BLOCKED);
    	cur->t_state &= ~T_BLOCKED;

        block_interrupts--;
	assert( block_interrupts == 0);
    	FM_enable_intr();

	return pan_thread_run_next;
}


void
pan_thread_schedule(void)
{
    /* Schedule another runnable thread (one with thread_id greater
     * than me). 
     */

    pan_thread_p t;

    assert(pan_thread_sched_busy);
    /* Service network interrupts first */
    if (pan_thread_queued_interrupt && block_interrupts == 0 &&
				      !(pan_handler_thread->t_state & T_FREE)) {
	assert( !(pan_handler_thread->t_state & T_RUNNABLE));
	pan_handler_thread->t_state |= T_RUNNABLE;
        block_interrupts++;
	t = pan_handler_thread;
    } else {
        /*
         * Try to find a runnable thread t. Since the search starts at the
         * next thread, we will only find the current thread when there is
         * no other runnable thread.
         */
        t = pan_cur_thread->t_next_active;
        while ( !(t->t_state & T_RUNNABLE)) {
            if (t == pan_cur_thread) {
#ifdef POLLING
    	        /* All threads are idle, start polling the network.
    	         */
		t = idle_loop(t);
#else
    	        /* All threads are idle, wait for an interrupt.
    	         */
		if (pan_thread_queued_interrupt) {
		    pan_handler_thread->t_state |= T_RUNNABLE;
        	    block_interrupts++;
		    t = pan_handler_thread;
		    break;
		}
                t = t->t_next_active;
#endif
    	    } else {
                t = t->t_next_active;
	    }
        }
     }
   
    /*
     * If there is no other runnable thread, then just continue
     * running the current thread.
     */
    if (t == pan_cur_thread) {
    	t->t_state |= T_RUNNING;
        return;
    }

    do_switch(t);
}
 
static void
do_switch( pan_thread_p t)
{
    pan_thread_p me = pan_cur_thread;

    if (me->t_state & T_FREE) {
	if (me == pan_handler_thread) {
	    pan_handler_thread = 0;
	    block_interrupts = 505;	/* Avoid fake interrupts by the
					 * signal_handler.
					 */
	}

	/* pull out of active thread link chain */
	me->t_prev_active->t_next_active = me->t_next_active;
	me->t_next_active->t_prev_active = me->t_prev_active;
	
	/* link into free thread list */
	me->t_next_active = free_threads;
	free_threads = me;
    }

    /*
     * Now we switch from pan_cur_thread to pan_next_thread.
     */
    me->t_state &= ~T_RUNNING;
    t->t_state |= T_RUNNING;
 
    assert(t->t_pc != 0);
    assert(pan_thread_sched_busy);
    STATINC( thread_switches);
    pan_cur_thread = t;
    pan_thread_switch(me, t);        /* do the switch; written in assembly */
    assert(pan_thread_sched_busy);
#ifndef NDEBUG
    pan_cur_thread->t_pc = 0;
#endif
}


void
pan_thread_run(pan_thread_p t)
{
    assert( t->t_state & T_RUNNABLE);
    do_switch( t);
}


/*****************************************************************/

#ifdef TIME_SLICING

static void
timer_handler(signum sig, thread_ustate *us, void *extra)
{
    pan_thread_p t;
    pan_time_t n, *now = &n;	/* should call create() */

    assert( sig == TIMER_SIGNAL);
    /* Amoeba ignores network signals when interrupted inside the
     * kernel (!), hence, we need to check the network regularly.
     */
    pan_thread_queued_interrupt = FM_pending();

    if ( !pan_thread_sched_busy) {
	sched_lock();

	/* walk active threads to decrease timer values */
	t = pan_cur_thread;
	do {
	    if ( t->t_state & T_TIMEOUT) {
	         pan_time_get(now);	/* inside if for efficiency */
		 if ( pan_time_cmp(now, t->t_timeout) > 0 &&
		      pan_mutex_trylock(t->t_cond->c_lock)) {
		    t->t_state &= ~T_TIMEOUT;
		    pan_cond_timeout(t->t_cond, t);
		    pan_mutex_unlock(t->t_cond->c_lock);
		} else {
		    ; /* try again later */
		}
	    }
	    t = t->t_next_active;
	} while (t != pan_cur_thread);

	/* force context switch;
	 * make sure we will return  and  we don't interrupt handler thread
	 */
        if ((pan_cur_thread->t_state & T_RUNNABLE) && block_interrupts == 0) {
	    pan_thread_schedule();
	    sched_unlock(1);
	} else {
	    sched_unlock(0);	/* do not check to avoid cntxt switch */
	}
    }
}

static void
timer_thread( char *param, int psize)
{
    while ( mu_trylock(&timer_lock, (interval)TIME_SLICE) != 0) {
	STATINC( timer_interrupts);
	sig_raise( TIMER_SIGNAL);
    }
    thread_exit();
}
#endif TIME_SLICING

static void
pan_thread_sleep(void)
{
    assert(pan_cur_thread == pan_handler_thread);
 
    pan_handler_thread->t_state &= (~T_RUNNING) & (~T_RUNNABLE);
    if (--block_interrupts == 0) {
    	FM_enable_intr();
    }
    pan_thread_schedule();
}


static void
handler_thread(void *arg)
{
    trc_new_thread(0, "handler_thread");
    sched_lock();
    while (network_poll) {
	pan_do_poll();
	pan_thread_sleep();
	assert( block_interrupts > 0);
    }
    sched_unlock(0);
    pan_thread_exit();
}


static void
signal_handler(signum sig, thread_ustate *us, void *extra)
{
    int busy;

    STATINC( network_interrupts);
    assert( sig == NETWORK_SIGNAL);
fake_interrupt:
    /* Check if we interrupt at a safe point:
     *   - critical sections are protected by a lock
     *   - don't interrupt the handler thread handling interrupts
     *   - make sure we interrupt a runnable thread (otherwise we will
     *	   never return to end the signal_handler!)
     */
    if ( !pan_thread_sched_busy) {
	sched_lock();
        if ( block_interrupts == 0 && (pan_cur_thread->t_state & T_RUNNABLE)) {
	    /* Lock out network interrupts until we have polled the network
	     * AND this signal handler has returned (to avoid stack overflow)
	     */
	    block_interrupts += 2;
	    assert( !(pan_handler_thread->t_state & T_RUNNABLE));
    	    pan_handler_thread->t_state |= T_RUNNABLE;
	    do_switch( pan_handler_thread);
	    busy = --block_interrupts;	/* test inside critical region */
	    sched_unlock(1);
	    if (busy == 0) {
	        /* A message may have arrived during the last context switch.
	         * Poll network now to avoid stack overflow.
	         */
	        if (FM_pending() > 0) {
		    goto fake_interrupt;
	        }
	        FM_enable_intr();		/* last action before return */
	    }
	    return;
        }
	sched_unlock(0);
    }
    STATINC( delayed_interrupts);
    pan_thread_queued_interrupt = 1;
    /* Don't enable interrupts because we can not handle them right now.
     * FM_enable_intr();
     */
}


static void
pan_do_poll(void)
{
    assert( pan_thread_sched_busy);

    do {
        pan_thread_queued_interrupt = 0;
        sched_unlock(0);
        STATINC( polls);
        FM_extract();			/* upcall outside critical section */
        sched_lock();
    } while (0);	/* (FM_pending() > 0);  NOT for latency test */

    /* Network interrupts must explicitely be enabled after a (sucessful)
     * call to FM_extract().
     */
    if (block_interrupts == 0) {
        FM_enable_intr();
    }
}

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
pan_thread_start_interrupts(void)
{
    network_poll = 1;
    block_interrupts++;			/* handler will be running */
    pan_handler_thread = pan_thread_create( handler_thread, 0, 0, 0, 0);

    /* make sure handler is initialized to avoid starvation when this thread
     * busy waits for a message from outside.
     */
    do
        pan_thread_yield();
    while (block_interrupts > 0);

    sig_catch(NETWORK_SIGNAL, signal_handler, (void *) NULL);
    _await_intr(0, NETWORK_SIGNAL);

#ifdef TIME_SLICING
    /* don't start the timer thread before network is properely initialized */
    thread_enable_preemption();
    mu_init(&timer_lock);
    mu_lock(&timer_lock);
    sig_catch(TIMER_SIGNAL, timer_handler, NULL);
    if (!thread_newthread(timer_thread, 8*1024, NULL, 0)) {
	printf( "%2d:thread_newthread failed\n", pan_sys_pid);
	abort();
    }
#endif
}


static void
thread_body(void)
{
    /* All threads start here */

    pan_thread_p t;

    assert( pan_thread_sched_busy);
    t = pan_cur_thread;
    sched_unlock(1);
    (*(t->t_func))(t->t_arg);

    pan_panic("%d) thread_body: thread never called pan_thread_exit()\n",
	       pan_sys_pid);
}

void
pan_sys_thread_option( char *str)
{
    pan_panic( "thread module: unknown option %s", str);
}

void
pan_sys_thread_start(void)
{
    int i;

    /*
     * pan_sys_thread_start() should only be called from the main
     * thread. Since this thread is not created by pan_thread_create(),
     * we explicitly build a thread structure for it, so that it can
     * safely call pan_thread_self().
     */

    /* Init thread code. Use thread 0 for the current process running. */
    pan_cur_thread = &pan_all_thread[0];
    pan_cur_thread->t_state = (~T_FREE & (T_RUNNING|T_RUNNABLE|T_DETACHED));
    pan_cur_thread->t_prio = 1;
    pan_cur_thread->t_func = 0;
    pan_cur_thread->t_arg = 0;
    pan_cur_thread->t_next = 0;
    pan_cur_thread->t_prev_active = pan_cur_thread;
    pan_cur_thread->t_next_active = pan_cur_thread;

    /* Use thread 1 for the idle loop (note: stack space is wasted). */
    pan_idle_thread->t_state = (~T_FREE & 0);
    pan_idle_thread->t_prio = -1;
    pan_idle_thread->t_func = (pan_thread_func_p) -1;
    pan_idle_thread->t_arg = (void *) -1;
    pan_idle_thread->t_next = 0;
    /* Link pan_idle_thread in the active thread list */
    pan_idle_thread->t_prev_active = pan_cur_thread->t_prev_active;
    pan_cur_thread->t_prev_active = pan_idle_thread;
    pan_idle_thread->t_prev_active->t_next_active = pan_idle_thread;
    pan_idle_thread->t_next_active = pan_cur_thread;

    free_threads = 0;

    for(i = 2; i < MAX_THREAD; i++) {
	pan_all_thread[i].t_state = T_FREE;
	pan_all_thread[i].t_next_active = free_threads;
	free_threads = &(pan_all_thread[i]);
    }

#ifdef DEBUG    
    my_address = pan_panic();
#endif
}

void
pan_sys_thread_end(void)
{
#ifdef TIME_SLICING
    mu_unlock(&timer_lock);
    threadswitch();	/* stop the timer thread now, so it won't garble I/O */
#endif
    sched_lock();
    while (pan_handler_thread->t_state & T_RUNNABLE)
    	pan_thread_schedule();	/* make sure the handler thread goes to sleep */
    network_poll = 0;
    block_interrupts++;		/* to avoid race with duplicate messages */
    pan_handler_thread->t_state |= T_RUNNABLE;
    sched_unlock(0);
    pan_thread_join( pan_handler_thread);
#ifdef THREAD_STATISTICS
    printf( "%2d: #ctxt sw = %d, #timer = %d, #netw = %d (delay %d), #polls = %d\n",
	    pan_sys_pid, thread_switches, timer_interrupts, network_interrupts,
	    delayed_interrupts, polls);
#endif
}

pan_thread_p
pan_thread_create(pan_thread_func_p func, void *arg, long stacksize, 
		  int prio, int detach)
{
    pan_thread_p t;
    struct sparc_frame *sp;
    int i;

    sched_lock();
    /* 
     * The requested stack size and priority are checked here, but
     * otherwise ignored.
     */
    if (stacksize > STACK_SIZE) {
	pan_panic("%d) pan_thread_create: "
		  "stack size (%ld) exceeds max. stack size (%ld)\n",
		  pan_sys_pid, stacksize, STACK_SIZE);
    }
    if (prio < 0 || prio > thread_maxprio) {
	pan_panic("pan_thread_create: illegal priority %d\n", prio);
    }

    /* Look for a free thread. */
    t = free_threads;
    free_threads = free_threads->t_next_active;

    assert(t);
    assert(t->t_state == T_FREE); /* require explicit clearing of all bits */

    /* Found a free thread; init it. */
    t->t_prio = prio;
    sp = (struct sparc_frame *)(t->t_stack +
			  SA(STACK_SIZE + BODY_OFFSET - MINFRAME));
    sp->fr_savfp = (struct sparc_frame *)(t->t_stack + SA(STACK_SIZE + BODY_OFFSET));
    t->t_sp = (long)sp;
#ifndef QPT
    t->t_pc = (long)thread_body - RETURN_OFFSET + 4;
#else
    t->t_pc = *(long *)((long)thread_body) - RETURN_OFFSET + 4;
#endif

    t->t_state = (detach ? (T_RUNNABLE|T_DETACHED) : T_RUNNABLE);
    t->t_func = func;
    t->t_arg = arg;
    t->t_next = 0;

    for ( i=0; i < MAX_KEY; i++) {
	t->t_glocal[i] = 0;
    }
    
    /* link it in just after?  before?  current one */
    t->t_prev_active = pan_cur_thread->t_prev_active;
    pan_cur_thread->t_prev_active = t;
    t->t_prev_active->t_next_active = t;
    t->t_next_active = pan_cur_thread;

    sched_unlock(1);
    return t;
}

void
pan_thread_clear(pan_thread_p thread)
{
}

void
pan_thread_exit()
{
    pan_thread_p me;

    sched_lock();
    me = pan_cur_thread;
    /*
     * If this is not a detached thread, then wait until it is
     * joined. Once it has been joined, make the joiner runnable.
     * We have to poll in between schedules,
     * else we may be switching back and forth between
     *  threads that are trying to exit...
     */
    if (!(me->t_state & T_DETACHED)) {
	while (!(me->t_state & T_JOINED)) {
	    pan_do_poll();
	    pan_thread_schedule();
	}
	pan_all_thread[me->t_joiner].t_state |= T_RUNNABLE;
    }

    me->t_state = T_FREE;

    pan_thread_schedule();	          /* schedule a new thread */
    abort();                              /* we should never get here! */
}

void
pan_thread_join(pan_thread_p thread)
{
    sched_lock();
    if (thread->t_state & T_DETACHED) {
	pan_panic("pan_thread_join: cannot join a detached thread\n");
    }
    thread->t_joiner = pan_cur_thread - pan_all_thread;
    thread->t_state |= T_JOINED;
    pan_cur_thread->t_state &= ~T_RUNNING;
    pan_cur_thread->t_state &= ~T_RUNNABLE;
    pan_thread_schedule();
    sched_unlock(1);
}

void
pan_thread_yield(void)
{
    /* don't suspend high priority threads! */
    if ( pan_cur_thread != pan_handler_thread &&
	 pan_cur_thread != pan_idle_thread) {
    	sched_lock();
    	pan_thread_schedule();
    	sched_unlock(0);	/* no recursion please */
    }
}

pan_thread_p
pan_thread_self(void)
{
    return pan_cur_thread;
}

int
pan_thread_getprio(void)
{
    return pan_cur_thread->t_prio;
}

int
pan_thread_setprio(int priority)
{
    int oldprio = pan_cur_thread->t_prio;

    if (priority < 0 || priority > thread_maxprio) {
	pan_panic("pan_thread_setprio: illegal priority %d\n", priority);
    }
    pan_cur_thread->t_prio = priority;

    return oldprio;
}
      
int
pan_thread_minprio(void)
{
    return 0;
}

int
pan_thread_maxprio(void)
{
    return MIN(100, thread_maxprio);       /* Should be sufficient */
}
