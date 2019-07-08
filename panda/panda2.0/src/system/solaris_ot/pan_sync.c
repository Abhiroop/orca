/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"		/* Provides a system interface */
#include "pan_sync.h"
#include "pan_time.h"
#include "pan_global.h"
#include "pan_error.h"
#include "sys/time.h"

#include "signal.h"

#include "ot_sync.c"		/* So the compiler can inline for performance */

typedef struct blckd {
	    struct blckd *forw, *backw;
	    int expired;
	    pan_cond_p cond;
	    pan_time_p time;
	} blocked_thr;

static blocked_thr *blocked_threads = NULL;
static pan_mutex_p timer_lock;
static pan_time_p now;


void
pan_sys_sync_start(void)
{
    now = pan_time_create();
    timer_lock = pan_mutex_create();
}


void
pan_sys_sync_end(void)
{
    pan_time_clear( now);
    pan_mutex_clear( timer_lock);
}


void pan_mutex_init( pan_mutex_p mutex)
{
    ot_queue_init (&mutex->blockq);
    ot_mutex_init(&mutex->mutex, &mutex->blockq);
}

pan_mutex_p
pan_mutex_create(void)
{
    pan_mutex_p mutex;

    mutex = (pan_mutex_p)pan_malloc(sizeof(struct pan_mutex));
    assert(mutex);

    pan_mutex_init( mutex);

    return mutex;
}


void
pan_mutex_clear(pan_mutex_p mutex)
{
    pan_free(mutex);
}


void
pan_mutex_lock(pan_mutex_p mutex)
{
    ot_mutex_lock(&mutex->mutex);
}


void
pan_mutex_unlock(pan_mutex_p mutex)
{
    ot_mutex_unlock(&mutex->mutex);
}


int
pan_mutex_trylock(pan_mutex_p mutex)
{
    return ot_mutex_trylock(&mutex->mutex);
}


pan_cond_p
pan_cond_create(pan_mutex_p mutex)
{
    pan_cond_p cond;

    cond = (pan_cond_p)pan_malloc(sizeof(struct pan_cond));
    assert(cond);

    ot_queue_init (&cond->blockq);
    ot_cond_init(&cond->cond, &cond->blockq, &mutex->mutex);

    cond->mutex = mutex;

    return cond;
}


void
pan_cond_clear(pan_cond_p cond)
{
    pan_free(cond);
}


void
pan_cond_wait(pan_cond_p cond)
{
    ot_cond_wait( &cond->cond);
}


	
void
pan_cond_signal(pan_cond_p cond)
{
    ot_cond_bcast(&cond->cond);
}


void
pan_cond_broadcast(pan_cond_p cond)
{
    ot_cond_bcast(&cond->cond);
}


void
check_for_timeouts(void)
{
    blocked_thr *thr;

    if ( blocked_threads && pan_mutex_trylock(timer_lock)) {
        pan_time_get( now);
 
        /* walk the unsorted chain of blocked threads */
        for (thr = blocked_threads; thr != NULL; thr = thr->forw) {
            if (pan_time_cmp(now, thr->time) > 0) {
                thr->expired = 1;
		if (pan_mutex_trylock( thr->cond->mutex)) {
                    pan_cond_broadcast( thr->cond);
		    pan_mutex_unlock( thr->cond->mutex);
		}
            }
        }
        pan_mutex_unlock(timer_lock);
    }
}


int
pan_cond_timedwait(pan_cond_p cond, pan_time_p abstime)
{
    blocked_thr me;

    pan_mutex_lock( timer_lock);
    me.time = abstime;
    me.cond = cond;
    me.expired = 0;
    me.forw = blocked_threads;
    me.backw = NULL;
    if (blocked_threads != NULL) {
	blocked_threads->backw = &me;
    }
    blocked_threads = &me;
    pan_mutex_unlock( timer_lock);

    pan_cond_wait( cond);

    pan_mutex_lock( timer_lock);
    if (me.forw) {
	me.forw->backw = me.backw;
    }
    if (me.backw) {
	me.backw->forw = me.forw;
    } else {
	blocked_threads = me.forw;
    }
    pan_mutex_unlock( timer_lock);

    return !me.expired;
}
