/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <assert.h>

#include "pan_sys.h"		/* Provides a system interface */

#include "pan_util.h"

#include "pan_time_fix2t.h"

void
pan_time_fix2t_start(void)
{
}
 
 
void
pan_time_fix2t_end(void)
{
}


void
pan_time_fix2t(pan_time_fix_p f, pan_time_p t)
{
    double d;

    d = pan_time_fix_t2d(f);
    pan_time_d2t(t, d);
}
