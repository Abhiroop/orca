/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_SOLARIS_SYNC_
#define _SYS_SOLARIS_SYNC_

#include <synch.h>

struct pan_mutex{
    mutex_t mutex;
};

struct pan_cond{
    cond_t       cond;
    pan_mutex_p  mutex;
};

extern void pan_sys_sync_start(void);
extern void pan_sys_sync_end(void);

#endif /* _SYS_SOLARIS_SYNC_ */
