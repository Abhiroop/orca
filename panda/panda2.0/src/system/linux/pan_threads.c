#include "pan_sys.h"
#include "pan_global.h"
#include "pan_threads.h"
#include "pan_error.h"

#include <pthread.h>

static pthread_attr_t attr, prio;
static pthread_mutex_t attr_mutex, prio_mutex;
static pthread_key_t self_key;
static pan_thread_p main_thread;

#define DEFAULT_STACKSIZE 102400 
#define DEFAULT_PRIORITY 0


/*
  Should be called before any other routine in this module. Initialises some
  data structures.
*/

void
pan_sys_thread_start(void)
{
    if (pthread_attr_init(&prio) != 0){
	pan_panic("attr init");
    }

    if (pthread_attr_init(&attr) != 0){
	pan_panic("attr init");
    }

    if (pthread_mutex_init(&prio_mutex, NULL) != 0){
	pan_panic("prio mutex create");
    }

    if (pthread_mutex_init(&attr_mutex, NULL) != 0){
	pan_panic("attr mutex create");
    }

    if (pthread_key_create(&self_key, NULL) != 0){
	pan_panic("self key create");
    }

    main_thread = pan_malloc(sizeof(struct pan_thread));

    if (pthread_setspecific(self_key, main_thread) != 0){
	pan_panic("key main thread");
    }
}

/*
  Should be called to end thread usage
*/
void
pan_sys_thread_end(void)
{
    if (pthread_mutex_destroy(&attr_mutex) != 0){
	pan_panic("attr mutex create");
    }
    if (pthread_mutex_destroy(&prio_mutex) != 0){
	pan_panic("prio mutex create");
    }

    if (pthread_attr_destroy(&attr) != 0){
	pan_panic("attr destroy");
    }

    if (pthread_attr_destroy(&prio) != 0){
	pan_panic("attr destroy");
    }
}


static void *
thread_wrapper(void *arg)
{
    pan_thread_p thread = arg;

    if (pthread_setspecific(self_key, thread) != 0){
	pan_panic("self key thread");
    }
    thread->func(thread->arg);

    return NULL;
}
 

pan_thread_p
pan_thread_create(void (*func)(void *arg), void *arg, long stacksize, 
		  int priority, int detach)
{
    pan_thread_p thread;
    size_t s;

    thread = pan_malloc(sizeof(struct pan_thread));

    if (stacksize == 0L){
	s = DEFAULT_STACKSIZE;
    }else{
	s = (size_t)stacksize;
    }

    if (pthread_mutex_lock(&attr_mutex) != 0){
	pan_panic("mutex_lock");
    }

    if (pthread_attr_setstacksize(&attr, s) != 0){
	pan_panic("set stack size");
    }

    if (priority == 0){
	priority = DEFAULT_PRIORITY;
    }

#ifdef NO
    if (pthread_attr_setprio(&attr, priority) != 0){
	pan_panic("set priority");
    }
#endif

    thread->func = func;
    thread->arg = arg;

    if (pthread_create(&thread->thread, &attr, thread_wrapper, thread) != 0){
	pan_panic("thread create");
    }

    if (pthread_mutex_unlock(&attr_mutex) != 0){
	pan_panic("mutex unlock");
    }

    return thread;
}

void
pan_thread_clear(pan_thread_p thread)
{
    pan_free(thread);
}

void
pan_thread_exit(void)
{
    pthread_exit(NULL);
}

void
pan_thread_join(pan_thread_p thread)
{
    void *result;

    if (pthread_join(thread->thread, &result) != 0){
	pan_panic("pthread join");
    }
}


void
pan_thread_yield(void)
{
    pthread_yield();
}

pan_thread_p
pan_thread_self(void)
{
    pan_thread_p thread;
 
    thread = (pan_thread_p)pthread_getspecific(self_key);

    return thread;
}

/* XXX: Check priority management in MIT pthreads  */

int
pan_thread_getprio(void)
{
#ifdef NO
    int r;

    if (pthread_mutex_lock(&prio_mutex) != 0){
	pan_panic("lock prio");
    }

    if (pthread_getschedattr(pthread_self(), &prio) != 0){
	pan_panic("get scheduling attributes");
    }

    if ((r = pthread_attr_getprio(&prio)) != 0){
	pan_panic("get priority");
    }

    if (pthread_mutex_unlock(&prio_mutex) != 0){
	pan_panic("prio unlock");
    }

    return(r);
#else
    return 0;
#endif
}


int
pan_thread_setprio(int priority)
{
#ifdef NO
    int ret;

    if (pthread_mutex_lock(&prio_mutex) != 0){
	pan_panic("lock prio");
    }

    if (pthread_getschedattr(pthread_self(), &prio) != 0){
	pan_panic("get scheduling attributes");
    }

    if ((ret = pthread_attr_getprio(&prio)) != 0){
	pan_panic("getting priority");
    }

    if (pthread_attr_setprio(&prio, priority) != 0){
	pan_panic("setting priority");
    }

    if (pthread_mutex_unlock(&prio_mutex) != 0){
	pan_panic("unlock prio");
    }

    return ret;
#else
    return 0;
#endif
}

/* XXX: check min/max prio */

int
pan_thread_minprio(void)
{
    return 0;
}

int
pan_thread_maxprio(void)
{
    return 100;
}
