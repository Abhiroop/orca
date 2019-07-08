/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_SYNC_
#define _SYS_GENERIC_SYNC_

struct pan_mutex{
};

struct pan_cond{
};

extern void pan_sys_sync_start(void);
extern void pan_sys_sync_end(void);

#endif
