/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"		/* Provides a system interface */
#include "pan_sync.h"

pan_mutex_p
pan_mutex_create(void)
{
}


void
pan_mutex_clear(pan_mutex_p lock)
{
}


void
pan_mutex_lock(pan_mutex_p lock)
{
}


void
pan_mutex_unlock(pan_mutex_p lock)
{
}


int
pan_mutex_trylock(pan_mutex_p lock)
{
}


pan_cond_p
pan_cond_create(pan_mutex_p lock)
{
}


void
pan_cond_clear(pan_cond_p cond)
{
}


void
pan_cond_wait(pan_cond_p cond)
{
}


int
pan_cond_timedwait(pan_cond_p cond, pan_time_p abstime)
{
}

	
void
pan_cond_signal(pan_cond_p cond)
{
}


void
pan_cond_broadcast(pan_cond_p cond)
{
}


