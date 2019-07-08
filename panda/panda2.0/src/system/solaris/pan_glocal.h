/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_AMOEBA_GLOCAL_
#define _SYS_AMOEBA_GLOCAL_

#include <thread.h>

struct pan_key{
    thread_key_t key;
};

extern void pan_sys_glocal_start(void);
extern void pan_sys_glocal_end(void);

#endif
