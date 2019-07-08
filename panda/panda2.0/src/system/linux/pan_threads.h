#ifndef _SYS_LINUX_THREAD_
#define _SYS_LINUX_THREAD_

#include <pthread.h> 

struct pan_thread{
    pthread_t thread;
    void    (*func)(void *arg);
    void     *arg;
};

extern void pan_sys_thread_start(void);
extern void pan_sys_thread_end(void);

#endif
