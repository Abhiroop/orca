/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*
 *  This module implements the sweeper for the mcast protocol.
 *
 *  Clients may register function+arg, which are called regularly.
 *  One lock protects all. A pointer to this lock is passed to this module
 *  in the init call.
 *  During the function call, this lock is held. If the function does an action
 *  for which the lock must be opened (such as communication), it must itself
 *  release the lock.
 */

#ifndef __SYS_MCAST_SWEEP_H__
#define __SYS_MCAST_SWEEP_H__

#include "pan_sys_msg.h"


typedef void (*pan_mcast_sweep_func)(void *);


void pan_mcast_sweep_register(pan_mcast_sweep_func f, void *arg);

void pan_mcast_sweep_init(pan_mutex_p lock, pan_time_p interval,
			  int stack, int prio);

void pan_mcast_sweep_end(void);


#endif
