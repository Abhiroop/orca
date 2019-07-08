/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"
#include "pan_util.h"
#include "pan_sleep.h"

#include <stdio.h>

static pan_mutex_p sleep_lock;
static pan_cond_p  sleep_cond;


/*------ Utility functions ---------------------------------------------------*/

void
pan_sleep(pan_time_p t)
{
    pan_time_p now = pan_time_create();

    pan_mutex_lock(sleep_lock);
    pan_time_get(now);
    pan_time_add(now, t);
    pan_cond_timedwait(sleep_cond, now);
    pan_mutex_unlock(sleep_lock);

    pan_time_clear(now);
}

void
pan_sleep_start(void)
{
    sleep_lock = pan_mutex_create();
    sleep_cond = pan_cond_create(sleep_lock);
}


void
pan_sleep_end(void)
{
    pan_cond_clear(sleep_cond);
    pan_mutex_clear(sleep_lock);
}
