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


/* acquire lock on behalf of thread t */
#ifdef __GNUC__
__inline__
#endif
void
pan_lock_acquire_internal(pan_mutex_p lock, pan_thread_p t)
{
    assert(lock->l_inited);
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
    assert( t->t_next == 0);
    t->t_state &= (~T_RUNNING) & (~T_RUNNABLE);
    if (pan_cur_thread == t) {
	pan_thread_schedule();
    }
}

#ifdef __GNUC__
__inline__
#endif
void
pan_mutex_init(pan_mutex_p lock)
{
    lock->l_taken = 0;
    lock->l_blocked = 0;
    lock->l_blocked_tail = 0;
#ifndef NDEBUG
    lock->l_inited = 1;
#endif
#ifdef LOCK_WRITE_OWNER
    lock->l_owner = NULL;
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
pan_mutex_lock(pan_mutex_p lock)
{
    if (!(lock->l_taken)) {
	lock->l_taken = 1;
#ifdef LOCK_WRITE_OWNER
	lock->l_owner = pan_cur_thread;
#endif
    } else {
	pan_lock_acquire_internal(lock, pan_cur_thread);
    }
}

#ifdef __GNUC__
__inline__
#endif
void
pan_mutex_do_unlock(pan_mutex_p lock)
{
    pan_thread_p t;
    
    assert(lock->l_inited);
    assert(lock->l_taken);
#ifdef LOCK_WRITE_OWNER
    assert(lock->l_owner == pan_cur_thread);
#endif
    assert(lock->l_blocked);

    t = lock->l_blocked;
    lock->l_blocked = t->t_next;
#ifdef LOCK_WRITE_OWNER
    lock->l_owner = t;
#endif
    t->t_next = 0;
    t->t_state |= T_RUNNABLE;
}


void
pan_mutex_unlock(pan_mutex_p lock)
{
    if (lock->l_blocked) {
	pan_mutex_do_unlock(lock);
    } else {
	lock->l_taken = 0;
#ifdef LOCK_WRITE_OWNER
	lock->l_owner = NULL;
#endif
    }
}

int
pan_mutex_trylock(pan_mutex_p lock)
{
    assert(lock->l_inited);
    if (!(lock->l_taken)) {
	lock->l_taken = 1;
#ifdef LOCK_WRITE_OWNER
	lock->l_owner = pan_cur_thread;
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

void
pan_cond_wait(pan_cond_p cond)
{
    pan_mutex_p lock = cond->c_lock;

    assert(cond);
    assert(cond->c_state & C_INIT);
    assert((cond->c_state & C_TAKEN) || (!cond->c_blocked));
    assert(lock);
    assert(lock->l_inited);
    assert(lock->l_taken);
#ifdef LOCK_WRITE_OWNER
    assert(lock->l_owner == pan_cur_thread);
#endif

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
    assert( pan_cur_thread->t_next == 0);
    pan_mutex_unlock(lock);
    pan_cur_thread->t_state &= (~T_RUNNING) & (~T_RUNNABLE);
    pan_thread_schedule();
    if (pan_cur_thread->t_state & T_SIGNAL_SELF) {
					/* Special case: we signalled ourself,
					 * so must take the lock */
	assert(! (lock->l_taken));
	pan_cur_thread->t_state &= ~T_SIGNAL_SELF;
	pan_mutex_lock(lock);
    }
    assert(lock->l_taken);
}

int
pan_cond_timedwait(pan_cond_p cond, pan_time_p abstime)
{
#ifdef POLL_ON_TIMEDWAIT
    volatile int i;
#endif

    /*
     * 
     */
    printf("%d) (warning) pan_cond_timedwait: not implemented properly\n",
	   pan_sys_pid);
#ifdef POLL_ON_TIMEDWAIT
    pan_mutex_unlock(cond->c_lock);
    pan_poll();
    for (i = 0; i < 1000000; i++) {
    }
    pan_poll();
    pan_mutex_lock(cond->c_lock);
    return 0;
#else
    pan_cond_wait(cond);
    return 0;
#endif
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
	/* Allow thread to signal itself (e.g. from a poll) */
	if (t == pan_cur_thread) {
	    t->t_state |= T_SIGNAL_SELF;
	} else {
	    /* acquire the lock on behalf of woken up thread */
	    pan_lock_acquire_internal(cond->c_lock, t);
	}
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
	/* Allow thread to signal itself (e.g. from a poll) */
	if (t == pan_cur_thread) {
	    t->t_state |= T_SIGNAL_SELF;
	} else {
	    /* acquire the lock on behalf of woken up thread */
	    pan_lock_acquire_internal(cond->c_lock, t);
	}
    }
    cond->c_blocked_tail = 0;
#ifndef NDEBUG
    cond->c_state &= ~C_TAKEN;
#endif
}
