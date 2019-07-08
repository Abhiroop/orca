/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"		/* Provides a system interface */
#include "pan_time.h"


void
pan_sys_time_start(void)
{
}
 
 
void
pan_sys_time_end(void)
{
}


pan_time_p
pan_time_create(void)
{
}

void
pan_time_clear(pan_time_p time)
{
}


void
pan_time_copy(pan_time_p to, pan_time_p from)
{
}


void
pan_time_get(pan_time_p now)
{
}

void
pan_time_set(pan_time_p time, long sec, unsigned long nsec)
{
}
 
int
pan_time_cmp(pan_time_p t1, pan_time_p t2)
{
}


void 
pan_time_add(pan_time_p res, pan_time_p delta)
{
}



void 
pan_time_sub(pan_time_p res, pan_time_p delta)
{
}


void 
pan_time_mul(pan_time_p res, int nr)
{
}



void 
pan_time_div(pan_time_p res, int nr)
{
}



void 
pan_time_mulf(pan_time_p res, double nr)
{
}


void
pan_time_d2t(pan_time_p t, double d)
{
}


double
pan_time_t2d(pan_time_p t)
{
}
