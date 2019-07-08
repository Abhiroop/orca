#ifndef _SYS_AMOEBA_THREAD_
#define _SYS_AMOEBA_THREAD_

#include "amoeba.h" 
#include "semaphore.h" 

struct pan_thread{
    long      prio;
    semaphore join;
    semaphore kill;
    semaphore prio_set;
    semaphore run;
    void    (*func)(void *arg);
    void     *arg;
    int       detached;
};

extern void pan_sys_thread_option( char *str);
extern void pan_sys_thread_start(void);
extern void pan_sys_thread_end(void);

#endif
