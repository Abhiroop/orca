/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_SOLARIS_SYNC_
#define _SYS_SOLARIS_SYNC_

#include "ot.h"

struct pan_mutex{
    ot_mutex_t mutex;
    ot_queue_t blockq;
};

struct pan_cond{
    ot_cv_t      cond;
    ot_queue_t   blockq;
    pan_mutex_p  mutex;
};

extern void pan_sys_sync_start(void);
extern void pan_sys_sync_end(void);

extern void pan_mutex_init(pan_mutex_p mutex);

#endif /* _SYS_SOLARIS_SYNC_ */
