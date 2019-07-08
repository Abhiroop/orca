/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_SOLARIS_TIME_
#define _SYS_SOLARIS_TIME_

#include <sys/time.h>

struct pan_time {
    timestruc_t time;
};

typedef struct pan_time pan_time_t;

extern void pan_sys_time_start(void);
extern void pan_sys_time_end(void);

#endif

