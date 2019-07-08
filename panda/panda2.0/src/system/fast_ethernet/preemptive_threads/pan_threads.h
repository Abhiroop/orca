#include "pan_sys.h"

#ifndef _SYS_ACTMSG_THREAD_
#define _SYS_ACTMSG_THREAD_

#include "pan_const.h"
#include "pan_asm.h"
#include "pan_time.h"

/* to remove the keyword __extension__ from stdarg expansion */
/* #define __extension__ */

/* HACK: make t_sp 8-byte aligned */
typedef struct pan_thread {
    long	      t_sp;        /* place to store sp on switch */
    long	      t_pc;	   /* place to store pc on switch */
    pan_thread_func_p t_func;      /* the thread function */
    void             *t_arg;       /* thread function argument */
    int		      t_state;	   /* state variable: T_FREE, etc. */
    int               t_prio;      /* thread priority: ignored */
    int		      t_joiner;    /* joiner id */
    pan_time_p	      t_timeout;   /* abs timeout value */
    pan_cond_p	      t_cond;      /* back pointer to condition variable */
    pan_thread_p      t_next;		
    pan_thread_p      t_next_active;  /* another active thread (next) */
    pan_thread_p      t_prev_active;  /* another active thread (previous) */

    /* At the back, so other fields can be accessed in 1 instruction */
    void             *t_glocal[MAX_KEY];        /* glocal data pointers */
    char	      t_stack[STACK_SIZE + BODY_OFFSET];      /* the stack */
} pan_thread_t;

#ifdef HIGHPRI
#define MAX_MSGS 4096
typedef struct message {
    /* make sure args are double aligned */
    int  args[4];
    void (*func)(void);
    int valid;
} message_t, *message_p;
#endif

extern pan_thread_t pan_all_thread[];  /* all thread descriptors */
extern pan_thread_p pan_cur_thread;

extern void pan_sys_thread_option( char *str);
extern void pan_sys_thread_start(void);
extern void pan_sys_thread_end(void);
extern void pan_thread_schedule(void);
extern void pan_thread_run(pan_thread_p t);

extern void pan_thread_disable_intr(void);
extern void pan_thread_enable_intr(void);

extern pan_thread_p pan_thread_run_next;

/* See if thread t should be run immediately because of its priority.
 * Current policy:
 *  - two levels of priority: network handlers run at high priority
 *                            Orca threads run at low priority
 *  - NEVER switch from high to low priority
 *  - switch between (Orca) threads of the same priority to get some
 *    fairness in scheduling
 */
#define pan_thread_prio_schedule(t) \
		if (pan_cur_thread->t_state & T_HANDLER) { \
		    pan_thread_run_next = (t); \
		} else { \
		    pan_thread_run( t); \
		}

/* Don't switch yet because the current thread still holds the mutex.
 */
#define pan_thread_hint_runnable(t)	pan_thread_run_next = (t)

/***************** Support for preemptive scheduling ******************/

extern volatile int pan_thread_sched_busy,
	            pan_thread_queued_interrupt;

extern void pan_thread_start_interrupts(void);

#define sched_lock()	(pan_thread_sched_busy = 1, flush_to_memory())

#define sched_unlock( check)	\
		(flush_to_memory(), pan_thread_sched_busy = 0, \
		(check && pan_thread_queued_interrupt ? pan_thread_yield() : 0))

/**********************************************************************/

#endif /* _SYS_ACTMSG_THREAD_ */
