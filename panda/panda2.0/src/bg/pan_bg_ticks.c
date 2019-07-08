#include <assert.h>
#include "pan_sys.h"
#include "pan_bg_ticks.h"
#include "pan_bg_global.h"
#include "pan_bg_history.h"
#include "pan_bg_send.h"

static pan_cond_p    cond;
static pan_thread_p  daemon;
static int           finish;

static pan_time_p inc;

static void
tdaemon(void *arg)
{
    int iter = 0;
    pan_time_p t;

    pan_mutex_lock(pan_bg_lock);

    t = pan_time_create();
    pan_time_get(t);

    while(!finish) {
	(void)pan_cond_timedwait(cond, t);
	pan_bg_history_tick(finish);
	pan_bg_send_tick(finish);

	if (++iter == 10) {
	    /* Refresh current time every second */
	    iter = 0;
	    pan_time_get(t);
	}
	pan_time_add(t, inc);
    }

    pan_time_clear(t);

    pan_mutex_unlock(pan_bg_lock);

    pan_thread_exit();
}
	    

void
pan_bg_ticks_start(void)
{
    inc = pan_time_create();
    pan_time_set(inc, 0L, 100000000L); /* 100 ms */

    cond = pan_cond_create(pan_bg_lock);
}


void
pan_bg_ticks_end(void)
{
    if (daemon){
	assert(finish);
    }

    pan_time_clear(inc);
}

void
pan_bg_ticks_run(void)
{
    daemon = pan_thread_create(tdaemon, NULL, 0, 3, 0);
}

void
pan_bg_ticks_release(void)
{
    /*
     * Tell timer daemon to quit. Caller has the lock.
     */

    if (daemon){
	finish = 1;
	pan_cond_broadcast(cond);

	/* Give daemon thread the opportunity to clean up. */
	pan_mutex_unlock(pan_bg_lock);
	pan_thread_join(daemon);
	pan_mutex_lock(pan_bg_lock);

	pan_cond_clear(cond);
    }
}
