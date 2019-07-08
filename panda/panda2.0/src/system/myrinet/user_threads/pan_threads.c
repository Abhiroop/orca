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


#ifdef STATISTICS
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



/*
 *		Global thread variables
 */
pan_thread_t pan_all_thread[MAX_THREAD];	/* all threads */
pan_thread_p pan_cur_thread;	  /* ptr to the current running thread */
pan_thread_p pan_next_thread;	  /* ptr to the next to be run; it is
				   * declared here because it is used in
				   * thread_switch (assembly).
				   */

static pan_thread_p free_threads;     /* free thread list */
static pan_thread_p dying_thread;

static long thread_maxprio = 100;

#ifdef DEBUG
static int thread_optimization = 0;
static int thread_contextswitch = 0;
#endif

static void thread_body(void);

extern void pan_thread_switch(void);   /* assembly routine */

#ifdef STATISTICS
static int switches = 0;
#endif

/* 
 * 			Thread code
 */

void
pan_thread_schedule(void)
{
    /* Schedule another runnable thread (one with thread_id greater
     * than me). 
     */

    pan_thread_p t;

    /*
     * Try to find a runnable thread t. Since the search starts at the
     * next thread, we will only find the current thread when there is
     * no other runnable thread.
     */
    t = pan_cur_thread->t_next_active;
    while( !(t->t_state & T_RUNNABLE)) {
        if (t == pan_cur_thread) {	/* Everybody idle */
	    /* What about the state of the current thread? */
	    pan_poll();
	}
        t = t->t_next_active;
    }
   
    /*
     * If there is no other runnable thread, then just continue
     * running the current thread.
     */
    if (t == pan_cur_thread && !dying_thread) {
    	t->t_state |= T_RUNNING;
        return;
    }
 
    assert(dying_thread || t != pan_cur_thread);
 
    /*
     * Tricky. If pan_thread_schedule() is called by some dying
     * thread, then that thread will have set pan_cur_thread to some
     * arbitrary active thread (see pan_thread_exit). In that case, we
     * should _not_ flush the state of the dying thread to
     * pan_cur_thread. We should first set pan_cur_thread to the dying
     * thread again, so that the flush will not garble the wrong
     * thread.
     */
    if (dying_thread) {
        pan_cur_thread = dying_thread;
        dying_thread = 0;
    }
 
    /*
     * Now we switch from pan_cur_thread to pan_next_thread.
     */
    pan_next_thread  = t;
    pan_cur_thread->t_state &= ~T_RUNNING;
    pan_next_thread->t_state |= T_RUNNING;
 
    STATINC(switches);
    pan_thread_switch();        /* do the switch; written in assembly */
}


/*****************************************************************/


static void
thread_body(void)
{
    /* All threads start here */

    pan_thread_p t;

    t = pan_cur_thread;
    (*(t->t_func))(t->t_arg);

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
    pan_cur_thread->t_next = 0;
    pan_cur_thread->t_state = (~T_FREE & (T_RUNNING|T_RUNNABLE|T_DETACHED));
    pan_cur_thread->t_prio = 1;
    pan_cur_thread->t_func = 0;
    pan_cur_thread->t_arg = 0;
    pan_cur_thread->t_prev_active = pan_cur_thread;
    pan_cur_thread->t_next_active = pan_cur_thread;

    free_threads = 0;

    for(i = 1; i < MAX_THREAD; i++) {
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
#ifdef DEBUG
    printf("thread:  context switch %d, optimization %d\n", 
	   thread_contextswitch, thread_optimization);
#endif
#ifdef STATISTICS
    printf("%2d: thr switches %d\n", pan_my_pid(), switches);
#endif
}

pan_thread_p
pan_thread_create(pan_thread_func_p func, void *arg, long stacksize, 
		  int prio, int detach)
{
    pan_thread_p t;
    struct sparc_frame *sp;

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
			  SA(STACK_SIZE + BODY_OFFSET - WINDOWSIZE));
    sp->fr_savfp = (struct sparc_frame *)(t->t_stack + SA(STACK_SIZE + BODY_OFFSET));
    t->t_sp = (long)sp;
#ifndef QPT
    t->t_pc = (long)thread_body - RETURN_OFFSET + 4;
#else
    t->t_pc = *(long *)((long)thread_body) - RETURN_OFFSET + 4;
#endif

    t->t_next = 0;
    t->t_state = (detach ? (T_RUNNABLE|T_DETACHED) : T_RUNNABLE);
    t->t_func = func;
    t->t_arg = arg;
    
    /* link it in just after?  before?  current one */
    t->t_prev_active = pan_cur_thread->t_prev_active;
    pan_cur_thread->t_prev_active = t;
    t->t_prev_active->t_next_active = t;
    t->t_next_active = pan_cur_thread;

    return t;
}

void
pan_thread_clear(pan_thread_p thread)
{
}

void
pan_thread_exit()
{
    pan_thread_p me = pan_cur_thread;

    /*
     * If this is not a detached thread, then wait until it is
     * joined. Once it has been joined, make the joiner runnable.
     * We have to poll in between schedules,
     * else we may be switching back and forth between
     *  threads that are trying to exit...
     */
    if (!(me->t_state & T_DETACHED)) {
	while (!(me->t_state & T_JOINED)) {
	    pan_poll();
	    pan_thread_schedule();
	}
	pan_all_thread[me->t_joiner].t_state |= T_RUNNABLE;
    }

    me->t_state = T_FREE;
    assert(me->t_prev_active != me);     /* make sure not last thread left */

    /* pull out of active thread link chain */
    me->t_prev_active->t_next_active = me->t_next_active;
    me->t_next_active->t_prev_active = me->t_prev_active;

    /* link into free thread list */
    me->t_next_active = free_threads;
    free_threads = me;

    /*
     * Reset 'pan_cur_thread' so pan_thread_schedule will work. It
     * needs an active thread to be current, plus it will start
     * looking at the thread just after pan_cur_thread.
     */
    dying_thread = me;
    pan_cur_thread = me->t_prev_active;
    pan_thread_schedule();	/* schedule a new thread */
    abort();                              /* we should never get here! */
}

void
pan_thread_join(pan_thread_p thread)
{
    if (thread->t_state & T_DETACHED) {
	pan_panic("pan_thread_join: cannot join a detached thread\n");
    }
    thread->t_joiner = pan_cur_thread - pan_all_thread;
    thread->t_state |= T_JOINED;
    pan_cur_thread->t_state &= ~T_RUNNING;
    pan_cur_thread->t_state &= ~T_RUNNABLE;
    pan_thread_schedule();
}

void
pan_thread_yield(void)
{
    pan_thread_schedule();
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
