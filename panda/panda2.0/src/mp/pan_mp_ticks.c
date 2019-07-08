/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <assert.h>
#include "pan_sys.h"
#include "pan_mp_ticks.h"

static pan_cond_p    cond;
static pan_mutex_p   lock;
static pan_thread_p  tdaemon_id;
static int           finish;
static void        (*handle_tick)(int finish);

static pan_time_p inc;

static void
tdaemon(void *arg)
{
    int iter = 0;
    pan_time_p t;

    pan_mutex_lock(lock);

    t = pan_time_create();
    pan_time_get(t);

    while(!finish) {
	(void)pan_cond_timedwait(cond, t);
	handle_tick(finish);
	if (++iter == 10) {
	    /* Refresh current time every second */
	    iter = 0;
	    pan_time_get(t);
	}
	pan_time_add(t, inc);
    }

    pan_time_clear(t);

    pan_mutex_unlock(lock);

    pan_thread_exit();
}
	    

void
pan_mp_ticks_start(void)
{
    inc = pan_time_create();
    pan_time_set(inc, 0L, 100000000L); /* 100 ms */
}


void
pan_mp_ticks_end(void)
{
    if (handle_tick){
	assert(finish);
    }

    pan_time_clear(inc);
}


void 
pan_mp_ticks_register(void (*handler)(int finish), pan_mutex_p m)
{
    assert(handle_tick == NULL);

    handle_tick = handler;
    finish      = 0;
    lock    = m;

    cond = pan_cond_create(lock);

    tdaemon_id = pan_thread_create(tdaemon, NULL, 0, 3, 0);
}

void
pan_mp_ticks_release(void)
{
    /*
     * Tell timer daemon to quit. Caller has the lock. The tick daemon
     * thread is joined in pan_mp_ticks_end.
     */

    if (handle_tick){
	finish = 1;
	pan_cond_broadcast(cond);

	/* Give daemon thread the opportunity to clean up. */
	pan_mutex_unlock(lock);
	pan_thread_join(tdaemon_id);
	pan_mutex_lock(lock);

	pan_cond_clear(cond);
    }
}
