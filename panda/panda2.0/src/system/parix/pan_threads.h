#ifndef _SYS_T800_THREAD_
#define _SYS_T800_THREAD_

#include <sys/link.h>
#include <sys/thread.h>

#include <setjmp.h>

#define PANDA_DATAKEYS_MAX  8           /* see 'pthreads.h' */

#define SYS_JOIN    0
#define SYS_DETACH  1


typedef void (*thread_func_p)(void *);

struct pan_thread {
    pan_thread_p detach_next;   	/* list of threads to detach */
    pan_thread_p glocal_next;   	/* list of all PANDA threads */
    pan_thread_p wake_up_next;  	/* threads waiting on cond_. */
    Thread_t    *parix_identification;

    LinkCB_t    *wake_up_link[2];	/* link to signal condition */
    Semaphore_t  wake_up_sema;		/* semaphore to signal cond */
    int          wake_up_modus;		/* LINK_MODUS or SEMA_MODUS */
    void        *glocal[PANDA_DATAKEYS_MAX];	/* pointers to glocal data */
    int          priority;
    int          detach; 
    thread_func_p panda_func;		/* panda-level thread main function */
    void        *panda_arg;		/* its return value */
    void        *result;
    jmp_buf      my_jmp_buf;		/* context for exit */
};


void         pan_sys_thread_start(void);

void         pan_sys_thread_end(void);


#define pan_thread_self()	((pan_thread_p)(GetLocal()))


#endif
