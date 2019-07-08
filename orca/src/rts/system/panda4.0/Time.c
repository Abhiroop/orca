/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: Time.c,v 1.2 1998/07/30 12:50:00 rutger Exp $ */

#include <orca_types.h>
#include <math.h>

#include "pan_sys.h"

#include "pan_util.h"

static pan_time_p	basetime;
static pan_time_p	tod;
static pan_mutex_p	timelock;

void
(ini_Time__Time)(void)
{
    static int done = 0;
    if (! done) {
	done = 0;
	ini_InOut__InOut();
    	basetime = pan_time_create();
    	tod = pan_time_create();
    	timelock = pan_mutex_create();
    	pan_time_get(basetime);
    }
}

int
f_Time__GetTime(void)
{
    double time;

    pan_mutex_lock(timelock);
    pan_time_get(tod);
    pan_time_sub(tod, basetime);
    time = pan_time_t2d(tod);      /* time in seconds */
    pan_mutex_unlock(timelock);

    return (int)floor(10 * time + 0.5);
}


int
f_Time__SysMilli(void)
{
    double time;

    pan_mutex_lock(timelock);
    pan_time_get(tod);
    pan_time_sub(tod, basetime);
    time = pan_time_t2d(tod);      /* time in seconds */
    pan_mutex_unlock(timelock);

    return (int)floor(1000.0 * time + 0.5);
}


t_real
f_Time__SysMicro(void)
{
    double time;

    pan_mutex_lock(timelock);
    pan_time_get(tod);
    pan_time_sub(tod, basetime);
    time = pan_time_t2d(tod);      /* time in seconds */
    pan_mutex_unlock(timelock);

    return 1000000.0 * time;
}


void
f_Time__Sleep(int sec, int nanosec)
{
    pan_time_p t = pan_time_create();

    pan_time_set(t, sec, nanosec);
    pan_sleep(t);
    pan_time_clear(t);
}
