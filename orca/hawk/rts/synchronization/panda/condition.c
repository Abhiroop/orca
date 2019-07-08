/*
 * Author:         Tim Ruhl
 *
 * Date:           Nov 30, 1995
 *
 * Condition synchronization
 *
 *           Implement the RTS condition synchronization interface on top
 *           of Panda. Condition synchronization takes place between 2
 *           threads. A condition synchronization variable is some sort
 *           of binary semaphore.
 */

#include <assert.h>

#include "synchronization.h"    /* Part of the synchronization module */
#include "panda_condition.h"
#include "pan_sys.h"
#include "panda_po_timer.h"
#include "util.h"

static int initialized;
static pan_time_p  add_time;
static pan_mutex_p time_lock;

int 
init_condition(int me, int group_size, int proc_debug)
{
    if (initialized++) return 0;

    add_time = pan_time_create();
    time_lock = pan_mutex_create();

    return 0;
}

int 
finish_condition(void)
{
    if (--initialized) return 0;

    pan_mutex_clear(time_lock);
    pan_time_clear(add_time);

    return 0;
}


condition_p 
new_condition(void)
{
    condition_p c;

    c = pan_malloc(sizeof(condition_t));

    c->lock = pan_mutex_create();
    c->cond = pan_cond_create(c->lock);
    c->time = pan_time_create();
    c->state = 0;

    return c;
}

int 
free_condition(condition_p c)
{
    c->state = 4567;
    pan_time_clear(c->time);
    pan_cond_clear(c->cond);
    pan_mutex_clear(c->lock);
    
    pan_free(c);

    return 0;
}


int 
signal_condition(condition_p c)
{
    pan_mutex_lock(c->lock);

    if (c->state > 0) {
	assert(c->state == 1);
	c->state = 0;
	pan_cond_signal(c->cond);
    }

    pan_mutex_unlock(c->lock);

    return 0;
}

int 
tawait_condition(condition_p c, po_time_t time, unit_t unit)
{
    int ret = -1;

    pan_mutex_lock(c->lock);

    if (c->state > 0 && time > 0) {
	pan_time_get(c->time);	/* current time */
	
	/* Compute absolute time */
	pan_mutex_lock(time_lock);
	rts_po_timer_time2panda(time, unit, add_time);
	pan_time_add(c->time, add_time);
	pan_mutex_unlock(time_lock);

	(void)pan_cond_timedwait(c->cond, c->time);
    }

    /* Can I have the lock now? */
    if (c->state == 0) {
	c->state = 1;		/* I take the lock */
	ret = 0;
    }	

    pan_mutex_unlock(c->lock);

    return ret;
}

int 
await_condition(condition_p c)
{
    pan_mutex_lock(c->lock);

    while(c->state > 0) {
	pan_cond_wait(c->cond);
    }

    assert(c->state == 0);
    c->state = 1;		/* I take the lock */

    pan_mutex_unlock(c->lock);

    return 0;
}

