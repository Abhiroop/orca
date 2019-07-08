#include "pan_sys.h"		/* Provides a system interface */
#include "pan_system.h"
#include "pan_sync.h"
#include "pan_threads.h"
#include "pan_time.h"
#include "pan_error.h"
#include "pan_global.h"
#include "pan_const.h"

#ifdef pan_mutex_lock
#undef pan_mutex_lock
#endif
#ifdef pan_mutex_unlock
#undef pan_mutex_unlock
#endif

void
pan_lock_acquire(struct pan_mutex *lock, pan_thread_p t)
{
    assert(lock->l_state & L_INIT);
    assert(lock->l_state & L_TAKEN);

    /* 
     * Don't want a thread to acquire twice.  Theoretically,
     * we should check for all threads on the blocked list.
     * Note that if we wanted to allow threads to call acquire twice 
     * but not end up on the list twice we would definitely have to
     * check the whole list.
     */
    assert(lock->l_owner != t - pan_all_thread);
    if (lock->l_blocked) {
	lock->l_blocked_tail->t_next = t;
    } else {
	lock->l_blocked = t;
    }
    lock->l_blocked_tail = t;
    t->t_next = 0;
    t->t_state &= (~T_RUNNING & ~T_RUNNABLE);
    if (pan_cur_thread == t) {
	pan_thread_schedule();
    }
}

pan_mutex_p
pan_mutex_create(void)
{
    pan_mutex_p lock;

    lock = (pan_mutex_p)pan_malloc(sizeof(struct pan_mutex));

    lock->l_blocked = 0;
    lock->l_blocked_tail = 0;
    lock->l_state = L_INIT;

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
    if (!(lock->l_state & L_TAKEN)) {
	lock->l_state |= L_TAKEN;
#ifndef NDEBUG
	lock->l_owner = pan_cur_thread - pan_all_thread;
#endif
    } else {
	pan_lock_acquire(lock, pan_cur_thread);
    }
}

void
pan_mutex_unlock(pan_mutex_p lock)
{
    pan_thread_p t;
    
    assert(lock->l_state & L_INIT);
    assert(lock->l_state & L_TAKEN);
    assert(lock->l_owner == pan_cur_thread - pan_all_thread);
    
    if (!lock->l_blocked) {
	lock->l_state &= ~L_TAKEN;
#ifndef NDEBUG
	lock->l_owner = -1;
#endif
    } else {
	t = lock->l_blocked;
	lock->l_blocked = t->t_next;
#ifndef NDEBUG
	lock->l_owner = t - pan_all_thread;
#endif
	t->t_next = 0;
	t->t_state |= T_RUNNABLE;
	if (t == pan_daemon) {       /* daemon has high prio while active! */
	    pan_thread_run_daemon();
	}
    }
}

int
pan_mutex_trylock(pan_mutex_p lock)
{
    assert(lock->l_state & L_INIT);
    if (!(lock->l_state & L_TAKEN)) {
	lock->l_state |= L_TAKEN;
#ifndef NDEBUG
	lock->l_owner = pan_cur_thread - pan_all_thread;
#endif
	return 1;
    }
    return 0;
}

pan_cond_p
pan_cond_create(pan_mutex_p lock)
{
    pan_cond_p cond;

    cond = (pan_cond_p)pan_malloc(sizeof(struct pan_cond));

    cond->c_blocked = 0;
    cond->c_blocked_tail = 0;
    cond->c_state = C_INIT;
    cond->c_lock = lock;

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
    pan_mutex_p lock = cond->c_lock;

    assert(cond);
    assert(cond->c_state & C_INIT);
    assert((cond->c_state & C_TAKEN) || (!cond->c_blocked));
    assert(lock);
    assert(lock->l_state & L_INIT);
    assert(lock->l_state & L_TAKEN);
    assert(lock->l_owner == pan_cur_thread - pan_all_thread);

#ifndef NDEBUG
    if (!(cond->c_state & C_TAKEN)) {
	cond->c_state |= C_TAKEN;
    }
#endif
    if (cond->c_blocked) {
	cond->c_blocked_tail->t_next = pan_cur_thread;
    } else {
	cond->c_blocked = pan_cur_thread;
    }
    cond->c_blocked_tail = pan_cur_thread;
    pan_cur_thread->t_state &= (~T_RUNNABLE) & (~T_RUNNING);
    pan_mutex_unlock(lock);                    /* may run the daemon! */
    pan_thread_schedule();
}

int
pan_cond_timedwait(pan_cond_p cond, pan_time_p abstime)
{
    volatile int i;

    /*
     * 
     */
    printf("%d) (warning) pan_cond_timedwait: not implemented properly\n",
	   pan_sys_pid);
    pan_mutex_unlock(cond->c_lock);
    pan_poll();
    for (i = 0; i < 1000000; i++) {
    }
    pan_poll();
    pan_mutex_lock(cond->c_lock);
    return 0;
}

void
pan_cond_signal(pan_cond_p cond)
{
    pan_thread_p t;

    assert(cond);
    assert(cond->c_state & C_INIT);

    if (cond->c_blocked) {
	t = cond->c_blocked;
	cond->c_blocked = t->t_next;
	t->t_next = 0;
	t->t_state |= T_RUNNABLE;
	/* 
	 * Acquire the lock on behalf of woken up thread. We assume
	 * that condition variables are only signalled by threads that
	 * hold the associated lock. Just to be sure, the following
	 * asserts are here.
	 */
	assert(cond->c_lock->l_state & L_TAKEN);
	assert(cond->c_lock->l_owner == pan_cur_thread - pan_all_thread);
	pan_lock_acquire(cond->c_lock, t);
    }
    if (!cond->c_blocked) {
#ifndef NDEBUG
	cond->c_state &= ~C_TAKEN;
#endif
	cond->c_blocked_tail = 0;
    }
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
	t->t_next = 0;
	t->t_state |= T_RUNNABLE;
	/* 
	 * Acquire the lock on behalf of woken up thread.  See also
	 * pan_cond_signal.
	 */
	assert(cond->c_lock->l_state & L_TAKEN);
	assert(cond->c_lock->l_owner == pan_cur_thread - pan_all_thread);
	pan_lock_acquire(cond->c_lock, t);
    }
    cond->c_blocked_tail = 0;
#ifndef NDEBUG
    cond->c_state &= ~C_TAKEN;
#endif
}
