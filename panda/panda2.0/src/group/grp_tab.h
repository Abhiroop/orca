/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Data structure to register and look-up groups
 */

#ifndef _GROUP_GRP_TAB_
#define _GROUP_GRP_TAB_

#include "pan_sys.h"

#include "pan_group.h"




void         pan_gtab_init(pan_mutex_p lock);

void         pan_gtab_clear(void);

pan_group_p  pan_gtab_add(pan_group_p g, int gid);

void         pan_gtab_delete(pan_group_p g, int gid);

pan_group_p  pan_gtab_locate(int gid);

void         pan_gtab_await_size(int n);

#endif
