#ifndef _SYS_ACTMSG_THREAD_
#define _SYS_ACTMSG_THREAD_

#include "pan_sys_msg.h"
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

extern void pan_sys_thread_start(void);
extern void pan_sys_thread_end(void);
extern void pan_thread_schedule(void);

#endif /* _SYS_ACTMSG_THREAD_ */
