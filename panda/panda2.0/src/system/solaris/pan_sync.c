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

void
pan_sys_sync_start(void)
{
}


void
pan_sys_sync_end(void)
{
}


pan_mutex_p
pan_mutex_create(void)
{
    pan_mutex_p mutex;

    mutex = (pan_mutex_p)pan_malloc(sizeof(struct pan_mutex));
    assert(mutex);

    if ((errno = mutex_init(&mutex->mutex, USYNC_THREAD, NULL)) != 0){
	pan_panic("mutex init");
    }

    return mutex;
}


void
pan_mutex_clear(pan_mutex_p mutex)
{
    if ((errno = mutex_destroy(&mutex->mutex)) != 0){
	pan_panic("mutex destroy");
    }

    pan_free(mutex);
}


void
pan_mutex_lock(pan_mutex_p mutex)
{
    if ((errno = mutex_lock(&mutex->mutex)) != 0){
	pan_panic("mutex lock");
    }
}


void
pan_mutex_unlock(pan_mutex_p mutex)
{
    if ((errno = mutex_unlock(&mutex->mutex)) != 0){
	pan_panic("mutex unlock");
    }
}


int
pan_mutex_trylock(pan_mutex_p mutex)
{
    int ret = 1;

    if ((errno = mutex_trylock(&mutex->mutex)) != 0){
	if (errno == EBUSY) ret = 0;
	else{
	    pan_panic("mutex trylock");
	}
    }

    return ret;
}


pan_cond_p
pan_cond_create(pan_mutex_p mutex)
{
    pan_cond_p cond;

    cond = (pan_cond_p)pan_malloc(sizeof(struct pan_cond));
    assert(cond);

    if ((errno = cond_init(&cond->cond, USYNC_THREAD, NULL)) != 0){
	pan_panic("cond init");
    }
    cond->mutex = mutex;

    return cond;
}


void
pan_cond_clear(pan_cond_p cond)
{
    if ((errno = cond_destroy(&cond->cond)) != 0){
	pan_panic("cond destroy");
    }

    pan_free(cond);
}


void
pan_cond_wait(pan_cond_p cond)
{
    while ((errno = cond_wait(&cond->cond, &cond->mutex->mutex)) == EINTR) {
    }

    if (errno != 0) {
        pan_panic("cond wait");
    }
}


int
pan_cond_timedwait(pan_cond_p cond, pan_time_p abstime)
{
    int ret = 0;

    while ((errno = cond_timedwait(&cond->cond, &cond->mutex->mutex, 
				   &abstime->time)) == EINTR) {
    }

    if (errno == ETIME) ret = 0;
    else if (errno == 0) ret = 1;
    else {
	pan_panic("cond timedwait");
    }

    return ret;
}

	
void
pan_cond_signal(pan_cond_p cond)
{
    if ((errno = cond_signal(&cond->cond)) != 0){
	pan_panic("cond signal");
    }
}


void
pan_cond_broadcast(pan_cond_p cond)
{
    if ((errno = cond_broadcast(&cond->cond)) != 0){
	pan_panic("cond broadcast");
    }
}


