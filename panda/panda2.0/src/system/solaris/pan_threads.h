/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_SOLARIS_THREAD_
#define _SYS_SOLARIS_THREAD_

#include <thread.h> 

struct pan_thread{
    thread_t thread;
    int      id;
    void   (*func)(void *arg);
    void    *arg;
};

extern void pan_sys_thread_start(void);
extern void pan_sys_thread_end(void);

extern void pan_sys_thread_upgrade(pan_thread_p thread);


#endif
