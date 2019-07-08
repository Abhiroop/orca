/* $Id: Time.c,v 1.8 1995/06/26 14:48:14 ceriel Exp $ */

#include <orca_types.h>
#include "panda/panda.h"

static timest_t first;
static mutex_t sleep_lock;
static cond_t sleep_cv;

void
(ini_Time__Time)(void)
{
    sys_gettime(&first);

    sys_mutex_init(&sleep_lock);
    sys_cond_init(&sleep_cv);
}

int
f_Time__GetTime(void)
{
    timest_t tod;

    sys_gettime(&tod);
    tod.t_sec -= first.t_sec;
    if (tod.t_nsec >= first.t_nsec) {
    	tod.t_nsec -= first.t_nsec;
    } else {
	tod.t_sec -= 1;
	tod.t_nsec += 1000000000 - first.t_nsec;
    }
    return (10 * tod.t_sec) + (tod.t_nsec / 100000000);     /* sec**-1 */
}


int
f_Time__SysMilli(void)
{
    timest_t tod;

    sys_gettime(&tod);
    tod.t_sec -= first.t_sec;
    if (tod.t_nsec >= first.t_nsec) {
    	tod.t_nsec -= first.t_nsec;
    } else {
	tod.t_sec -= 1;
	tod.t_nsec += 1000000000 - first.t_nsec;
    }
    return (1000 * tod.t_sec) + (tod.t_nsec / 1000000);     /* sec**-3 */
}


t_real
f_Time__SysMicro(void)
{
    timest_t tod;

    sys_gettime(&tod);
    tod.t_sec -= first.t_sec;
    if (tod.t_nsec >= first.t_nsec) {
    	tod.t_nsec -= first.t_nsec;
    } else {
	tod.t_sec -= 1;
	tod.t_nsec += 1000000000 - first.t_nsec;
    }
    return (1000000.0 * tod.t_sec) + (tod.t_nsec / 1000);     /* sec**-6 */
}


void
f_Time__Sleep(int sec, int nanosec)
{
    timest_t    t;
    timest_t    now;

    sys_gettime(&now);
    t.t_sec = sec;
    t.t_nsec = nanosec;
    time_add(&now, now, t);

    sys_mutex_lock(&sleep_lock);
    sys_cond_timedwait(&sleep_cv, &sleep_lock, &t);
    sys_mutex_unlock(&sleep_lock);
}
