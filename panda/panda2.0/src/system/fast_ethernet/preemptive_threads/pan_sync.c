#include "pan_sys_msg.h"		/* Provides a system interface */
#include "pan_system.h"
#include "pan_sync.h"
#include "pan_threads.h"
#include "pan_time.h"
#include "pan_error.h"
#include "pan_global.h"
#include "pan_const.h"


#ifdef pan_mutex_lock
#  undef pan_mutex_lock
#endif
#ifdef pan_mutex_unlock
#  undef pan_mutex_unlock
#endif
#ifdef pan_mutex_unlock_atomic
#  undef pan_mutex_unlock_atomic
#endif

/* acquire lock on behalf of thread t */
#ifdef __GNUC__
__inline__
#endif
static void
pan_lock_acquire_internal(pan_mutex_p lock, pan_thread_p t, int block)
{
    assert(lock < (pan_mutex_p) &end || lock->l_inited);
    assert(lock->l_taken);
#ifdef LOCK_WRITE_OWNER
    assert(lock->l_owner != t);
#endif

    /* 
     * Don't want a thread to acquire twice.  Theoretically,
     * we should check for all threads on the blocked list.
     * Note that if we wanted to allow threads to call acquire twice 
     * but not end up on the list twice we would definitely have to
     * check the whole list.
     */

    if (lock->l_blocked) {
	lock->l_blocked_tail->t_next = t;
    } else {
	lock->l_blocked = t;
    }
    lock->l_blocked_tail = t;
    t->t_next = 0;
    if (block) {
        t->t_state &= (~T_RUNNING) & (~T_RUNNABLE);
	pan_thread_schedule();
	assert( lock->l_taken);
#ifdef LOCK_WRITE_OWNER
	assert( lock->l_owner == pan_cur_thread);
#endif
#ifndef NDEBUG
	{ pan_thread_p s = lock->l_blocked;
	  while ( s) {
	      assert( s != t);
	      s = s->t_next;
	  }
	}
#endif
    }
}

void
pan_mutex_init(pan_mutex_p lock)
{
    lock->l_taken = 0;
    lock->l_blocked = 0;
    lock->l_blocked_tail = 0;
#ifdef LOCK_WRITE_OWNER
    lock->l_owner = 0;
#endif
#ifndef NDEBUG
    lock->l_inited = 1;
#endif
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
pan_mutex_do_lock(pan_mutex_p lock)
{
    sched_lock();
    /* Check again because some other thread may released the lock before
     * we disabled the interrupts.
     */
    if (pan_mutex_tas(lock->l_taken) == 0) {
#ifdef LOCK_WRITE_OWNER
	assert( lock->l_owner == NULL);
	assert( ! pan_cur_thread || (pan_cur_thread->t_state & T_RUNNABLE));
        lock->l_owner = pan_cur_thread;
#endif
    } else {
        pan_lock_acquire_internal(lock, pan_cur_thread, 1);
    }
    sched_unlock(1);
}

void
pan_mutex_lock(pan_mutex_p lock)
{
    assert(lock < (pan_mutex_p) &end || lock->l_inited);
    if (pan_mutex_tas(lock->l_taken) == 0) {
#ifdef LOCK_WRITE_OWNER
	assert( lock->l_owner == NULL);
	assert( ! pan_cur_thread || (pan_cur_thread->t_state & T_RUNNABLE));
	lock->l_owner = pan_cur_thread;
#endif
    } else {
	pan_mutex_do_lock(lock);
    }
}

void
pan_mutex_do_unlock(pan_mutex_p lock)
{
    pan_thread_p t;

    if (pan_mutex_tas(lock->l_taken) == 0) {
	/* Check again if some thread is (still) blocked */
        if (lock->l_blocked) {
    	    sched_lock();
	    /* pass lock to blocked thread */
    	    t = lock->l_blocked;
    	    lock->l_blocked = t->t_next;
#ifndef NDEBUG
    	    t->t_next = 0;
#endif
    	    t->t_state |= T_RUNNABLE;
#ifdef LOCK_WRITE_OWNER
    	    lock->l_owner = t;
#endif
	    /* Make sure handler thread runs when possible */
	    pan_thread_prio_schedule(t);
    	    sched_unlock(1);
    	}
	else {
            pan_mutex_release(lock->l_taken);
    	}
    }
}


void
pan_mutex_unlock(pan_mutex_p lock)
{
    assert(lock < (pan_mutex_p) &end || lock->l_inited);
#ifdef LOCK_WRITE_OWNER
    assert(lock->l_owner == pan_cur_thread);
    lock->l_owner = NULL;
#endif
    pan_mutex_release(lock->l_taken);

    if (lock->l_blocked) {
	pan_mutex_do_unlock(lock);
    }
}

void
pan_mutex_unlock_atomic(pan_mutex_p lock)
{
    pan_thread_p t;

    assert(pan_thread_sched_busy);
    assert(lock < (pan_mutex_p) &end || lock->l_inited);
#ifdef LOCK_WRITE_OWNER
    assert(lock->l_owner == pan_cur_thread);
    lock->l_owner = NULL;
#endif
    /* Scheduler lock is already taken so we can immediately test another
     * thread is waiting for the mutex.
     */
    if (lock->l_blocked) {
	/* pass lock to blocked thread */
    	t = lock->l_blocked;
    	lock->l_blocked = t->t_next;
#ifndef NDEBUG
    	t->t_next = 0;
#endif
    	t->t_state |= T_RUNNABLE;
#ifdef LOCK_WRITE_OWNER
    	lock->l_owner = t;
#endif
	/* Don't schedule now to maintain atomicity for pan_cond_wait() */
	/* pan_thread_prio_schedule(t); */
    } else {
        pan_mutex_release(lock->l_taken);
    }
}

int
pan_mutex_trylock(pan_mutex_p lock)
{
    assert(lock < (pan_mutex_p) &end || lock->l_inited);
    if (pan_mutex_tas(lock->l_taken) == 0) {
#ifdef LOCK_WRITE_OWNER
	assert( lock->l_owner == NULL);
	/* Hmm, the following assert fails in the case that the idle thread
	 * does not poll, but waits for an interrupt to occur. The timer
	 * interrupt then runs on a sleeping thread, calls trylock => error.
	 *
	 * assert( ! pan_cur_thread || (pan_cur_thread->t_state & T_RUNNABLE));
	 */
	lock->l_owner = pan_cur_thread;
#endif
	return 1;
    }
    return 0;
}

/* BUG report: condition structures may be handled without grabbing the
 * scheduler lock, because a the "user" must hold the associated mutex
 * that protects the condition variable. HOWEVER, when signalling a
 * sleeping thread, the scheduler lock must be obtained since queueing
 * the woken thread on the mutex can interfere with asynchronous actions
 * on the same mutex (e.g. an upcall accessing a locked object in the Orca
 * RTS).
 */

pan_cond_p
pan_cond_create(pan_mutex_p lock)
{
    pan_cond_p cond;

    cond = (pan_cond_p)pan_malloc(sizeof(struct pan_cond));

    cond->c_blocked = 0;
    cond->c_blocked_tail = 0;
#ifndef NDEBUG
    cond->c_state = C_INIT;
#endif
    cond->c_lock = lock;

    return cond;
}

void
pan_cond_clear(pan_cond_p cond)
{
    pan_free(cond);
}

/* pan_cond_wait() has been moved to pan_threads.c for optimal usage of
 * the -mflat option
 */

int
pan_cond_timedwait(pan_cond_p cond, pan_time_p abstime)
{
    int signalled;

    pan_cur_thread->t_timeout = abstime;
    pan_cur_thread->t_state |= T_TIMEOUT;
    pan_cur_thread->t_cond = cond;
    pan_cond_wait( cond);
    signalled = (pan_cur_thread->t_state & T_TIMEOUT);
    pan_cur_thread->t_state &= ~T_TIMEOUT;
    return signalled;
}

void
pan_cond_signal(pan_cond_p cond)
{
    pan_thread_p t;

    assert(cond);
    assert(cond->c_state & C_INIT);

    if (cond->c_blocked) {
	/* Code already assumes that signals may only be send when holding
	 * the corresponding mutex. Therefore the signalled thread will
	 * not become runnable, hence, no need to be concerned about interrupts.
	 */
	t = cond->c_blocked;
	cond->c_blocked = t->t_next;
#ifndef NDEBUG
	t->t_next = 0;
#endif
	t->t_state |= T_RUNNABLE;
        pan_thread_hint_runnable(t);
    }
#ifndef NDEBUG
    if (!cond->c_blocked) {
	cond->c_state &= ~C_TAKEN;
	cond->c_blocked_tail = 0;
    }
#endif
}

void
pan_cond_broadcast(pan_cond_p cond)
{
    pan_thread_p t;

    assert(cond);
    assert(cond->c_state & C_INIT);

    while (cond->c_blocked) {
	t = cond->c_blocked;
	cond->c_blocked = t->t_next;
#ifndef NDEBUG
	t->t_next = 0;
#endif
	t->t_state |= T_RUNNABLE;
        pan_thread_hint_runnable(t);
    }
#ifndef NDEBUG
    cond->c_blocked_tail = 0;
    cond->c_state &= ~C_TAKEN;
#endif
}

void
pan_cond_timeout(pan_cond_p cond, pan_thread_p t)
{
    pan_thread_p p;

    if (cond->c_blocked == t) {
	cond->c_blocked = t->t_next;
    } else {
	p = cond->c_blocked;
	while (p->t_next != t)
	    p = p->t_next;
	p->t_next = t->t_next;
	if (cond->c_blocked_tail == t)
	    cond->c_blocked_tail = p;
    }
#ifndef NDEBUG
    t->t_next = 0;
#endif
    t->t_state |= T_RUNNABLE;
    pan_thread_hint_runnable(t);
}


void
pan_sys_sync_start(void)
{
}

void
pan_sys_sync_end(void)
{
}
