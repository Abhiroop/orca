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
#include <string.h>


#if defined(THREAD_STATISTICS) && !defined(DO_TIMINGS)
#define DO_TIMINGS
#endif
#define USE_TIMER_FUNCTIONS

#include "pan_sys_msg.h"		/* Provides a system interface */
#include "pan_system.h"
#include "pan_threads.h"
#include "pan_global.h"
#include "pan_error.h"
#include "pan_const.h"
#include "pan_sync.h"
#include "pan_timer.h"

#include "pan_trace.h"

#include "amoeba.h"
#include "module/mutex.h"
#include "thread.h"
#include "exception.h"
#include "fault.h" /* From src/h/machdep/arch/<architecture> */
#include "module/signals.h"
#include "module/syscall.h"

#include "fm.h"

/* These prototypes have not made it to the public fm.h yet: */
extern void FM_print_lcp_mcgroup(int);
extern void FM_print_mc_credit(void);
extern void _await_intr(int, int);
extern void FM_enable_intr(void);
extern void FM_disable_intr(void);
extern void FM_set_parameter(int, ...);
extern unsigned int FM_pending(void);

int FM_enable[10];
int FM_disable[10];

/*
#define do_FM_enable_intr(i)	do {FM_enable[i]++; FM_enable_intr();} while (0)
#define do_FM_disable_intr(i)	do {FM_disable[i]++; FM_disable_intr();} while (0)
*/
#define do_FM_enable_intr(i)	FM_enable_intr()
#define do_FM_disable_intr(i)	FM_disable_intr()

#ifdef THREAD_STATISTICS
#  define STATINC(n)	(++(n))
#else
#  define STATINC(n)
#endif
#define NOSTATINC(n)


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


/* Use a negative signal since it avoids signal blocking
 * overhead which we can do without.
 */
#define NETWORK_SIGNAL	((signum) -123)
#define TIMER_SIGNAL    ((signum) 111)        /* negative sig is not allowed */

#define TIME_SLICE	100	/* msec */

/*
 *		Global thread variables
 */
pan_thread_t pan_all_thread[MAX_THREAD];	/* all threads */
pan_thread_p pan_cur_thread;	   /* ptr to the current running thread */
pan_thread_p pan_thread_run_next;  /* used by the idle-loop */


static pan_thread_p free_threads;     /* free thread list */

static int handler_active;	  /* to avoid recursive interrupts */

static long thread_maxprio = 100;

#ifdef TIME_SLICING
static mutex timer_lock;	         /* signals timer_thread to stop */
#endif

#ifdef SKIP_ACTIVE_POLL
static int pan_thread_active_poll = 0;	/* do a non-idle poll? */
#endif

volatile int pan_thread_sched_busy,      /* protects critical scheduling code */
             pan_thread_queued_interrupt;/* records delayed interrupts */

#ifdef THREAD_STATISTICS
static int thread_switches = 0;
static int stack_splits = 0;
static int network_interrupts = 0;
static int delayed_interrupts = 0;
static int polls = 0;
/*
static int timer_interrupts = 0;
*/

static int pan_thread_intr = 1;
static int pan_thread_poll = 1;
static int pan_thread_verbose = 0;
static int pan_thread_statistics = 0;

static pan_time_p t_start;
static pan_time_p t_stop;
static pan_timer_p idle_timer;
#else
#define pan_thread_intr		1
#define pan_thread_poll		1
#define pan_thread_verbose	0
#define pan_thread_statistics	0
#endif
#ifndef NDEBUG
static pan_thread_p switch_hist[128];
static int switch_indx = 0;
#endif

static int running_idle = 0;

/* assembly routine */
extern void pan_thread_switch(pan_thread_p cur, pan_thread_p next); 

static void thread_body(void);

static void pan_do_poll(int idle);

static void do_switch( pan_thread_p next);

static void split_stack(pan_thread_p blocked);


/* 
 * 			Thread code
 */

static pan_thread_p cached_thread = 0;	/* one spare thread in the run queue */

static pan_thread_p get_thread(void)
{
    pan_thread_p t;

    if (cached_thread) {
	t = cached_thread;
	cached_thread = 0;
    } else {
        assert( free_threads);
        t = free_threads;
        free_threads = free_threads->t_next_active;

	/* require explicit clearing of all bits */
        assert(t->t_state == T_FREE);

        /* link it in just after?  before?  current one */
        t->t_prev_active = pan_cur_thread;
        t->t_next_active = pan_cur_thread->t_next_active;
        pan_cur_thread->t_next_active->t_prev_active = t;
        pan_cur_thread->t_next_active = t;
    }

#ifndef NDEBUG
    t->t_pc = 0;
#endif
    return t;
}

static void free_thread( pan_thread_p t)
{
    if (cached_thread) {
        t->t_state = T_FREE;

        /* pull out of active thread link chain */
        t->t_prev_active->t_next_active = t->t_next_active;
        t->t_next_active->t_prev_active = t->t_prev_active;
	
        /* link into free thread list */
        t->t_next_active = free_threads;
        free_threads = t;
    } else {
	t->t_state = 0;		/* cheapest way to clear runnable bit */
	cached_thread = t;
    }
}


#define t_handler        t_func

extern int  pan_asm_get_sp(void);
extern void pan_thread_switch_fast( void (* fun)(void),
				    int stack, pan_thread_p blocked);

static
void split_top(void)
{
    sched_lock();

    /* The interrupt handler has been turned into a full blown thread,
     * which needs to be terminated properly.
     */

    if (pan_thread_poll) pan_thread_queued_interrupt = FM_pending();
    if (pan_thread_intr && !pan_thread_queued_interrupt) do_FM_enable_intr(0);

    handler_active = 0;
    pan_cur_thread->t_state = T_FREE;
    pan_thread_schedule();
}


__inline__ static void
pan_do_poll(int idle)
/*
 * Whenever we poll, it is possible that the upcall blocks (on a mutex).
 * To support blocking threads in a general and efficient way, we use the
 * optimistic policy of performing the poll on the current stack
 * and creating a separate thread only in the case of a blocking upcall.
 *
 * This routine can be called in two cases: the current thread is idle OR
 * the current thread is interrupted by a signal handler. In both cases
 * we need to allocate a fresh thread struct to record the status of the
 * upcall. However, we execute on top of the current thread's stack.
 *
 * This saves context switches if the upcall executes without blocking.
 *
 * If an upcall blocks, the state of the upcall thread is copied to its
 * own stack. Also the blocked thread is set free by modifying the call
 * stack to return to a special function that takes care of the exceptional
 * return path.
 */
{
    pan_thread_p new, blocked;
    int newsp;
    int verbose = pan_thread_verbose;
 
    assert( pan_thread_sched_busy);

    /* get a new thread struct for the message handler
     */
    new = get_thread();
    new->t_state = T_RUNNABLE | T_RUNNING | T_HANDLER | T_DETACHED;
    newsp = (int)(new->t_stack + STACK_SIZE);

    /* block the current thread
     */
    blocked = pan_cur_thread;
    blocked->t_state |= T_BLOCKED;
#ifndef NDEBUG
    blocked->t_state &= ~T_RUNNING;
#endif
				    /* needed to split the stack */
    blocked->t_handler = (pan_thread_func_p)new;

    pan_cur_thread = new;

    /* assert(!handler_active); This thread package supports blocking upcalls */
    handler_active = (int) new;

#ifndef NDEBUG
    assert(switch_hist[(switch_indx+127)%128] != pan_cur_thread);
    switch_hist[switch_indx++] = pan_cur_thread;
    switch_indx %= 128;
#endif
 
    /* poll the network, blocking will cause the stack to be split
     */
    if (idle) {
        pan_thread_run_next = 0;
	/* sys_null(); */
    }
    do {
        sched_unlock(0);		/* upcall outside critical section */

#ifdef THREAD_STATISTICS
	if (idle && verbose) {
	    /* measure looping until a message arrives or a timer expires
	     */
	    pan_timer_start( idle_timer);
	    while (!FM_pending() && pan_thread_run_next == 0) {}
	    pan_timer_stop( idle_timer);
	}
#endif

        pan_thread_queued_interrupt = 0;
FM_disable[1]++;
        pan_thread_switch_fast( FM_extract, newsp, blocked);	  /* upcall? */

	if (pan_thread_sched_busy) {		/* A stack split occurred */
	    assert(pan_cur_thread == blocked);
	    if (idle) {
		pan_thread_run_next = pan_cur_thread;
	    }
	    return;
	}

	/* NO stack split
	 */
	sched_lock();
        assert( pan_cur_thread == new);
	assert( new->t_state & T_HANDLER);
    }
    while (idle && pan_thread_run_next == 0);

    /* We are done and need to return the 'new' thread struct to
     * the free list.
     */
     
    free_thread( pan_cur_thread);

    /* Network interrupts must explicitely be enabled after a
     * (sucessful) call to FM_extract().
     */
    if (pan_thread_intr) do_FM_enable_intr(1);
    handler_active = 0;
    
    /* Resume execution of the blocked thread (i.e. return from this
     * function).
     */
    assert( blocked->t_state & T_BLOCKED);
    blocked->t_state &= ~T_BLOCKED;
    pan_cur_thread = blocked;

#ifndef NDEBUG
    assert(switch_hist[(switch_indx+127)%128] != pan_cur_thread);
    switch_hist[switch_indx++] = pan_cur_thread;
    switch_indx %= 128;
#endif
}


__inline__ static
void split_stack(pan_thread_p blocked)
{
    pan_thread_p handler = (pan_thread_p) blocked->t_handler;
    struct sparc_frame *sp;
 
    assert( handler->t_state & T_HANDLER);

    STATINC(stack_splits);

    if (handler == pan_cur_thread) {
	/* Flush the register windows so we can patch the stack of
	 * the handler thread.
	 */
	sys_null();
    }

    /* Relocate last pc so handler thread will CALL split_top() iso. returning
     * to pan_do_poll(). [CALL to get a new stack frame]
     */
    sp = (struct sparc_frame *) (handler->t_stack + STACK_SIZE - SA(MINFRAME));

    assert( blocked->t_sp == (int) sp->fr_savfp);

    sp->fr_savfp = sp;		/* so the handler will finish on its own stack */
#ifndef QPT
    sp->fr_savpc = ((int) split_top) - RETURN_OFFSET;
#else
    sp->fr_savpc = (*(int *) split_top)-RETURN_OFFSET;
#endif
 
    blocked->t_state &= ~T_BLOCKED;
}


__inline__ static void
do_switch( pan_thread_p next)
{
    pan_thread_p me = pan_cur_thread;

    if (next->t_state & T_BLOCKED) {
	/* Hmm, bad luck, the blocked thread must be resumed => split stack
	 */
	split_stack(next);
    }
    if (me->t_state & T_FREE) {
	/* This thread struct is no longer needed, so return it to the pool
	 */
	free_thread( me);
    }

    /*
     * Now we switch from pan_cur_thread to pan_next_thread.
     */
    STATINC( thread_switches);
#ifndef NDEBUG
    me->t_state &= ~T_RUNNING;
    next->t_state |= T_RUNNING;

    assert(switch_hist[(switch_indx+127)%128] != next);
    switch_hist[switch_indx++] = next;
    switch_indx %= 128;
#endif
    assert(next->t_pc != 0);
    assert(pan_thread_sched_busy);

    pan_cur_thread = next;
    pan_thread_switch(me, next);	/* do the switch; written in assembly */

    assert(pan_thread_sched_busy);
    assert(pan_cur_thread->t_state & T_RUNNING);
    assert(pan_cur_thread->t_state & T_RUNNABLE);
#ifndef NDEBUG
    pan_cur_thread->t_pc = 0;
#endif
}


void
pan_thread_schedule(void)
{
    /* Schedule another runnable thread (one with thread_id greater
     * than me). 
     */

    pan_thread_p t;
    int idle = 0;
		
    assert(pan_thread_sched_busy);

    /* Hack to keep code size small. Only call pan_do_poll()
     * at one place, at the expense of less clear code.
     */
    for (;;) {
        /* Service network interrupts first
	 */
        while (
#ifdef SKIP_ACTIVE_POLL
		pan_thread_active_poll &&
#else
		pan_thread_queued_interrupt && !handler_active
#endif
		) {
poll:	    pan_do_poll(idle);
	    if (idle && (pan_thread_run_next->t_state & T_RUNNABLE)) {
		t = pan_thread_run_next;
		goto found_it;
	    }
	}
        /* Try to find a runnable thread t. Since the search starts at the
         * next thread, we will only find the current thread when there is
         * no other runnable thread.
         */
loop:   t = pan_cur_thread;
	do {
	    t = t->t_next_active; 
            if (t->t_state & T_RUNNABLE) {
		goto found_it;
	    }
	}
	while ( t != pan_cur_thread);

    	/* Alll threads are idle, start polling the network.
	 */
	if (!idle) {
	    idle = 1;
if (pan_thread_poll) {
	    do_FM_disable_intr(2);	/* pan_do_poll will switch them on again! */
} else {
	    running_idle = 1;
	    pan_timer_start( idle_timer);
	    sched_unlock(0);	/* so interrupt can be handled immediately */
}
	}
if (pan_thread_poll) {
	goto poll;
} else {
        if (pan_thread_queued_interrupt) {
	    sched_lock();
	    pan_timer_stop( idle_timer);
	    running_idle = 0;
	    idle = 0;
	    goto poll;
	} else {
	    goto loop;
	}
}
     }

found_it:   

if (!pan_thread_poll && idle) {
    sched_lock();
    pan_timer_stop( idle_timer);
    running_idle = 0;
}
    assert( t->t_state & T_RUNNABLE);

    /* If there is no other runnable thread, then just continue
     * running the current thread.
     */
    if (t == pan_cur_thread) {
#ifndef NDEBUG
    	t->t_state |= T_RUNNING;
#endif
        return;
    }

    do_switch(t);
}


void
pan_thread_run(pan_thread_p t)
{
    assert( t->t_state & T_RUNNABLE);
    do_switch(t);
}

/*****************************************************************/


void
pan_poll(void)
{
    STATINC( polls);
    if (FM_pending()) {		/* test first since most polls fail! */
        sched_lock();
        if (!handler_active) {
            pan_do_poll(0);
        }
        sched_unlock(1);
    }
}
 

#ifdef TIME_SLICING

static void
timer_handler(signum sig, thread_ustate *us, void *extra)
{
    pan_thread_p t;
    pan_time_t n, *now = &n;	/* should call create() */
    static int cnt = 0;

    assert( sig == TIMER_SIGNAL);

    if ( pan_thread_statistics && cnt++ == 10 * 1000 / TIME_SLICE) {
    	printf( "%2d: #ints = %6d (%4d), #ctxt sw = %6d, #splits = %5d\n",
	   	pan_sys_pid, network_interrupts, delayed_interrupts,
		thread_switches, stack_splits);
	cnt = 0;
    }

    if ( !pan_thread_sched_busy) {
	sched_lock();

        /* Amoeba ignores network signals when interrupted inside the
         * kernel (!), hence, we need to check the network regularly.
         */
	if (pan_thread_intr)  {
	    if (pan_thread_poll || running_idle) {
	        pan_thread_queued_interrupt = FM_pending();
	    } else {
	        do_FM_enable_intr(3);
	    }
	}
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
        if ( pan_cur_thread->t_state & T_RUNNABLE && !handler_active) {
	    pan_thread_schedule();
	}
	sched_unlock(0);	/* do not check to avoid cntxt switch */
    }
}

static void
timer_thread( char *param, int psize)
{
    while ( mu_trylock(&timer_lock, (interval)TIME_SLICE) != 0) {
	NOSTATINC( timer_interrupts);
	sig_raise( TIMER_SIGNAL);
    }
    thread_exit();
}
#endif TIME_SLICING


static void
signal_handler(signum sig, thread_ustate *us, void *extra)
{
    STATINC( network_interrupts);
    assert( sig == NETWORK_SIGNAL);
if (!FM_pending()) FM_enable[9]++;

    /* Check if we interrupt at a safe point:
     *   - critical sections are protected by a lock
     *   - we don't interrupt an interrupt handler (can't handle nesting)
     *
     * Note that we may interrupt a non-runnable thread because either
     * the interrupt handler runs to completion without a stack split or
     * the underlying thread is made runnable. Hence, the return from interrupt
     * code will always be executed.
     */
    if ( !pan_thread_sched_busy) {
        sched_lock();
	if (!handler_active) {
	    if (running_idle) pan_timer_stop( idle_timer);
	    pan_do_poll(0);
 	    if (running_idle) pan_timer_start( idle_timer);
            sched_unlock(0);
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

void pan_thread_disable_intr(void)
{
    do_FM_disable_intr(4);
    pan_thread_intr = 0;
}

void pan_thread_enable_intr(void)
{
    /* To avoid getting duplicate interrupts in hand coded benchmark programs
     * extract all pending messages from the network.
     */
    if (FM_pending()) {
	pan_poll();
    }
    pan_thread_intr = 1;
    do_FM_enable_intr(5);
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
#ifdef SKIP_ACTIVE_POLL
    if (strcmp( str, "active_poll") == 0) {
	pan_thread_active_poll = 1;
    } else
#endif
	pan_panic( "thread module: unknown option %s", str);
}

void
pan_thread_start_interrupts(void)
{
    sig_catch(NETWORK_SIGNAL, signal_handler, (void *) NULL);
    _await_intr(NETWORK_DEVICE, NETWORK_SIGNAL);
    if (pan_thread_intr) do_FM_enable_intr(6);

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
#ifdef THREAD_STATISTICS
    pan_time_get( t_start);
#endif
}


static void
thread_body(void)
{
    /* All threads start here */

    pan_thread_p t;
    pan_thread_func_p func;

    assert( pan_thread_sched_busy);
    t = pan_cur_thread;
    func = t->t_func;	    /* Grr, t_func is also used by fast switching code */
    sched_unlock(1);
    (*func)(t->t_arg);

    pan_panic("%d) thread_body: thread never called pan_thread_exit()\n",
	       pan_sys_pid);
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

    free_threads = 0;

    for(i = 1; i < MAX_THREAD; i++) {
	pan_all_thread[i].t_state = T_FREE;
	pan_all_thread[i].t_next_active = free_threads;
	free_threads = &(pan_all_thread[i]);
    }

#ifdef THREAD_STATISTICS
    idle_timer = pan_timer_create();
    t_start = pan_time_create();
    t_stop = pan_time_create();
#endif
}

void
pan_sys_thread_end(void)
{
    double frac;

#ifdef TIME_SLICING
    mu_unlock(&timer_lock);
    threadswitch();	/* stop the timer thread now, so it won't garble I/O */
#endif
#ifdef THREAD_STATISTICS
    pan_time_get( t_stop);
    pan_time_sub( t_stop, t_start);
    if (pan_timer_read( idle_timer, t_start) > 0) {
        frac = pan_time_t2d( t_start) / pan_time_t2d( t_stop);
    } else {
	frac = 0.0;
    }

    if ( pan_thread_verbose) {
    	printf( "%2d: #ints = %5d, #ctxt sw = %5d, #splits = %4d, "
		"#polls = %6d, idle = %4.2f%%\n",
	   	pan_sys_pid, network_interrupts, thread_switches, stack_splits,
		polls, 100.0 * frac);

	{int i;
	for (i=0; i < 10; i++)
	    if (FM_enable[i] + FM_disable[i] > 0) {
	        printf( "%d: enable[%d] %5d, disable  %5d\n", pan_sys_pid, i,
		        FM_enable[i], FM_disable[i]);
	    }
	}
    }
    pan_time_clear( t_start);
    pan_time_clear( t_stop);
    pan_timer_clear( idle_timer);
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

    t = get_thread();
    t->t_prio = prio;
    sp = (struct sparc_frame *)(t->t_stack +
			  SA(STACK_SIZE - BODY_OFFSET - MINFRAME));
    sp->fr_savfp = (struct sparc_frame *)(t->t_stack + SA(STACK_SIZE - BODY_OFFSET));
    t->t_sp = (long)sp;
#ifndef QPT
    t->t_pc = (long)thread_body - RETURN_OFFSET;
#else
    t->t_pc = (*(long *)thread_body) - RETURN_OFFSET;
#endif

    t->t_state = (detach ? (T_RUNNABLE|T_DETACHED) : T_RUNNABLE);
    t->t_func = func;
    t->t_arg = arg;
    t->t_next = 0;

    for ( i=0; i < MAX_KEY; i++) {
	t->t_glocal[i] = 0;
    }
    
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
	    /* pan_do_poll(0);	save code space, an interrupt will occur ...  */
	    /* Hmm, interrupts can be disabled by the user => deadlock.
	     * Fix by setting pan_thread_queued_interrupt, which will cause a
	     * poll inside pan_thread_schedule() if a message is pending.
	     */
	    pan_thread_queued_interrupt = FM_pending();
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
    if (!(pan_cur_thread->t_state & T_HANDLER)) {
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
