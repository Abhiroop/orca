/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_SOLARIS_THREAD_
#define _SYS_SOLARIS_THREAD_

#include "ot.h"

#define PAN_THREAD_DETACHED	0x1
#define PAN_THREAD_DEAD		0x2
#define PAN_THREAD_EXITED	0x4

#define MAX_KEY		8

struct pan_thread{
    ot_thread_t tcb;
    int      prio;
    int      flags;
    int      id;
    void   (*func)(void *arg);
    void    *arg;
    void    *t_glocal[MAX_KEY];
};

extern void pan_sys_thread_start(void);
extern void pan_sys_thread_end(void);

extern void pan_sys_thread_upgrade(pan_thread_p thread);

#endif
