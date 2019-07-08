#ifdef FAST
#include "pan_threads_fast.c"
#else
#include "pan_threads_old.c"
#endif

/* Koen: performance hack so pan_cond_wait can be compiled using the -mflat
 * option without slowing down other (leaf) functions in pan_sync.c
 */

#include "pan_sys_msg.h"

extern end;

void
pan_cond_wait(pan_cond_p cond)
{
    pan_mutex_p lock = cond->c_lock;
    pan_thread_p me = pan_cur_thread;	/* help GCC  :-( */

    assert(cond);
    assert(cond->c_state & C_INIT);
    assert((cond->c_state & C_TAKEN) || (!cond->c_blocked));
    assert(lock);
    assert(lock < (pan_mutex_p) &end || lock->l_inited);
    assert(lock->l_taken);
#ifdef LOCK_WRITE_OWNER
    assert(lock->l_owner == me);
#endif

#ifndef NDEBUG
    if (!(cond->c_state & C_TAKEN)) {
	cond->c_state |= C_TAKEN;
    }
#endif
    if (cond->c_blocked) {
	cond->c_blocked_tail->t_next = me;
    } else {
	cond->c_blocked = me;
    }
    me->t_next = 0;
    cond->c_blocked_tail = me;
    /* Grab scheduler lock before releasing the mutex, so we can go to sleep
     * atomically.
     */
    sched_lock();
    pan_mutex_unlock_atomic(lock);
    me->t_state &= (~T_RUNNABLE) & (~T_RUNNING);
    pan_thread_schedule();
#ifdef CHECK_IN_COND_WAIT
    sched_unlock(1);		/* increase speed by skipping check */
#else
    sched_unlock(0);		/* increase speed by skipping check */
#endif
    pan_mutex_lock(lock);
}
