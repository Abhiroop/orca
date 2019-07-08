#ifndef _SYS_CMAML_THREAD_
#define _SYS_CMAML_THREAD_

#include "pan_sys.h"
#include "pan_const.h"

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
    void             *t_glocal[MAX_KEY];        /* glocal data pointers */
    pan_thread_p      t_next;		
    pan_thread_p      t_next_active;  /* another active thread (next) */
    pan_thread_p      t_prev_active;  /* another active thread (previous) */
    char	      t_stack[STACK_SIZE + BODY_OFFSET];      /* the stack */
} pan_thread_t;

extern pan_thread_t pan_all_thread[];  /* all thread descriptors */
extern pan_thread_p pan_cur_thread;
extern pan_thread_p pan_daemon;

void pan_sys_thread_start(void);
void pan_sys_thread_end(void);
void pan_lock_acquire(pan_mutex_p lock, pan_thread_p t);
void pan_thread_schedule(void);
void pan_thread_daemon(pan_thread_p d);
void pan_thread_run_daemon(void);

#endif /* _SYS_CMAML_THREAD_ */
