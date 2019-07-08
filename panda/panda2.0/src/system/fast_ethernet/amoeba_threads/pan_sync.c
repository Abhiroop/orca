#include "pan_sys_msg.h"		/* Provides a system interface */
#include "pan_sync.h"
#	undef pan_mutex_lock
#	undef pan_mutex_unlock
#	undef pan_mutex_trylock
#include "pan_time.h"
#include "pan_global.h"
#include "pan_error.h"

#ifdef __GNUC__
__inline__
#endif
void pan_mutex_init(pan_mutex_p lock)
{
    mu_init(&lock->lock);
}

pan_mutex_p
pan_mutex_create(void)
{
    pan_mutex_p lock;

    lock = (pan_mutex_p)pan_malloc(sizeof(struct pan_mutex));
    pan_mutex_init(lock);

    return lock;
}


void
pan_mutex_clear(pan_mutex_p lock)
{
    pan_free(lock);
}


void
pan_mutex_lock(pan_mutex_p lock)
{
    mu_lock(&lock->lock);
}


void
pan_mutex_unlock(pan_mutex_p lock)
{
    mu_unlock(&lock->lock);
}


int
pan_mutex_trylock(pan_mutex_p lock)
{
    return !mu_trylock(&lock->lock, (interval)0);
}


pan_cond_p
pan_cond_create(pan_mutex_p lock)
{
    pan_cond_p cond;

    cond = (pan_cond_p)pan_malloc(sizeof(struct pan_cond));

    /*
     * cond->sentinel.lock is not used!
     */
    cond->sentinel.prio = -1;
    cond->sentinel.next = &cond->sentinel;
    cond->lock = lock;

    return cond;
}


void
pan_cond_clear(pan_cond_p cond)
{
    assert(cond->sentinel.next == &cond->sentinel);
    pan_free(cond);
}


void
pan_cond_wait(pan_cond_p cond)
{
    struct waiter w, *p, *prev;

    mu_init(&w.lock);
    mu_lock(&w.lock);   /* lock once, second lock below puts us to sleep */
    w.prio = pan_thread_getprio();
    
    p = cond->sentinel.next;
    prev = &cond->sentinel;

    while (p->prio >= w.prio) {
	prev = p;
	p = p->next;
    }
    w.next = p;
    prev->next = &w;

    mu_unlock(&cond->lock->lock);  /* let go of critical section */
    mu_lock(&w.lock);              /* put myself to sleep */
    mu_lock(&cond->lock->lock);    /* someone woke us up; 
				    * grab critical section.
				    */
}


int
pan_cond_timedwait(pan_cond_p cond, pan_time_p abstime)
{
    struct waiter w, *p, *prev;
    struct pan_time now;
    interval msec;
    int timeout;

    amoeba_gettime(&now);
    pan_time_sub(abstime, &now);
    msec = (interval)(1000.0 * pan_time_t2d(abstime));
    pan_time_add(abstime, &now);
    if (msec <= 0) {
	return 0;
    }

    mu_init(&w.lock);
    mu_lock(&w.lock);   /* lock once, second lock below puts us to sleep */
    w.prio = pan_thread_getprio();
    
    p = cond->sentinel.next;
    prev = &cond->sentinel;

    while (p->prio >= w.prio) {
	prev = p;
	p = p->next;
    }
    w.next = p;
    prev->next = &w;

    mu_unlock(&cond->lock->lock);         /* release critical section */
    timeout = mu_trylock(&w.lock, msec);  /* sleep for a while */
    mu_lock(&cond->lock->lock);           /* someone woke us up; 
					   * grab critical section
					   */
    if (timeout) {
	/* Are we still in the queue? */
	for (prev = &cond->sentinel; prev->next != &cond->sentinel; prev = prev->next) {
	    /*
	     * Find w's predecessor.
	     */
	    if ( prev->next == &w) {
		prev->next = w.next;
		break;
	    }
	}
    }

    return !timeout;
}

	
void
pan_cond_signal(pan_cond_p cond)
{
    struct waiter *head, *tail;

    head = cond->sentinel.next;
    tail = &cond->sentinel;

    if (head != tail) {
	tail->next = head->next;
	mu_unlock(&head->lock);
    }
}


void
pan_cond_broadcast(pan_cond_p cond)
{
    struct waiter *p;

    for (p = cond->sentinel.next; p != &cond->sentinel; p = p->next) {
	mu_unlock(&p->lock);
    }
    cond->sentinel.next = &cond->sentinel;
}


