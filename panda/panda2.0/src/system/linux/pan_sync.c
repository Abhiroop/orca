#include "pan_sys.h"
#include "pan_global.h"
#include "pan_sync.h"
#include "pan_error.h"
#include "pan_time.h"

#include <pthread.h>
#include <errno.h>
#include <string.h>

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

    mutex = pan_malloc(sizeof(pan_mutex_t));
    
    memset(&mutex->mutex, 0, sizeof(pthread_mutex_t));
    if (pthread_mutex_init(&mutex->mutex, NULL) != 0){
	pan_panic("mutex init");
    }

    return mutex;
}

void
pan_mutex_clear(pan_mutex_p mutex)
{
    if (pthread_mutex_destroy(&mutex->mutex) != 0){
	pan_panic("mutex destroy");
    }

    pan_free(mutex);
}

void
pan_mutex_lock(pan_mutex_p mutex)
{
    if (pthread_mutex_lock(&mutex->mutex) != 0){
	pan_panic("mutex lock");
    }
}


void
pan_mutex_unlock(pan_mutex_p mutex)
{
    if (pthread_mutex_unlock(&mutex->mutex) != 0){
	pan_panic("mutex unlock");
    }
}


int
pan_mutex_trylock(pan_mutex_p mutex)
{
    int ret = 1;

    /* ###erik: on linux, pthread_mutex_trylock returns errno, instead of
       setting it.  */
    if ((errno = pthread_mutex_trylock (&mutex->mutex)) != 0) {
        if (errno == EBUSY) ret = 0;
        else {
            pan_panic("mutex trylock");
        }
    }

    return (ret);
}

pan_cond_p
pan_cond_create(pan_mutex_p mutex)
{
    pan_cond_p cond;

    cond = pan_malloc(sizeof(struct pan_cond));

    memset(&cond->cond, 0, sizeof(pthread_cond_t));
    if (pthread_cond_init(&cond->cond, NULL) != 0){
	pan_panic("cond init");
    }

    cond->mutex = mutex;

    return cond;
}

void
pan_cond_clear(pan_cond_p cond)
{
    if (pthread_cond_destroy(&cond->cond) != 0){
	pan_panic("cond destroy");
    }

    pan_free(cond);
}


void
pan_cond_wait(pan_cond_p cond)
{
    if (pthread_cond_wait(&cond->cond, &cond->mutex->mutex) != 0){
	pan_panic("cond wait");
    }
}

int
pan_cond_timedwait(pan_cond_p cond, pan_time_p abstime)
{
    int ret = 1;

    if (pthread_cond_timedwait(&cond->cond, &cond->mutex->mutex, 
			       &abstime->time) != 0){
	if (errno == ETIMEDOUT) ret = 0;
	else{
	    pan_panic("cond timedwait");
	}
    }

    return(ret);
}


void
pan_cond_signal(pan_cond_p cond)
{
    if (pthread_cond_signal(&cond->cond) != 0){
	pan_panic("cond signal");
    }
}

void
pan_cond_broadcast(pan_cond_p cond)
{
    if (pthread_cond_broadcast(&cond->cond) != 0){
	pan_panic("cond broadcast");
    }
}
