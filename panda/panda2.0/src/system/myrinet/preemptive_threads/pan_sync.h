#ifndef _SYS_ACTMSG_SYNC_
#define _SYS_ACTMSG_SYNC_

#include <assert.h>
#include "pan_threads.h"
#include "pan_sys_msg.h"
#include "pan_asm.h"

/*	#ifndef NDEBUG */
/*	#  define LOCK_WRITE_OWNER */
/*	#endif */


#define L_INIT   	0x1
#define L_TAKEN		0x2

struct pan_mutex {
    int		 l_taken;
    pan_thread_p l_blocked;
    pan_thread_p l_blocked_tail;
#ifndef NDEBUG
#endif
    int		 l_inited;
#ifdef LOCK_WRITE_OWNER
#endif
    pan_thread_p l_owner;  /* only valid if (l_taken | l_inited)) */
};
    
#define C_INIT   	0x1
#define C_TAKEN		0x2

struct pan_cond {
    pan_thread_p c_blocked;
    pan_thread_p c_blocked_tail;
    pan_mutex_p  c_lock;
#ifndef NDEBUG
#endif
    int		 c_state;
};

extern end;
    
extern void pan_sys_sync_start(void);
extern void pan_sys_sync_end(void);
extern void pan_cond_timeout(pan_cond_p cond, pan_thread_p t);

extern void pan_mutex_init(pan_mutex_p lock);
extern void pan_mutex_do_lock(pan_mutex_p lock);
extern void pan_mutex_do_unlock(pan_mutex_p lock);
extern void pan_mutex_unlock_atomic(pan_mutex_p lock);

 
#define pan_mutex_tas(flag)		tas( &(flag))
#define pan_mutex_release(flag)		((flag) = 0, flush_to_memory())
 
#ifndef NO_SYNC_MACROS

#define pan_mutex_lock Pan_mutex_lock
#define pan_mutex_unlock Pan_mutex_unlock
#define pan_mutex_unlock_atomic Pan_mutex_unlock_atomic

#ifdef __GNUC__
__inline__
#endif
static void
Pan_mutex_lock(pan_mutex_p lock)
{
    assert(lock->l_inited);
    if (pan_mutex_tas(lock->l_taken) == 0) {
#ifdef LOCK_WRITE_OWNER
	assert( lock->l_owner == NULL);
        lock->l_owner = pan_cur_thread;
#endif
    } else {
        pan_mutex_do_lock(lock);
    }
}

#ifdef __GNUC__
__inline__
#endif
static void
Pan_mutex_unlock(pan_mutex_p lock)
{
    assert(lock->l_inited);
#ifdef LOCK_WRITE_OWNER
    assert( lock->l_owner == pan_cur_thread);
    lock->l_owner = NULL;
#endif
    pan_mutex_release(lock->l_taken);
 
    if (lock->l_blocked) {
        pan_mutex_do_unlock(lock);
    }
}

#ifdef __GNUC__
__inline__
#endif
static void
Pan_mutex_unlock_atomic(pan_mutex_p lock)
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

#endif		/* NO_SYNC_MACROS */

#endif /* _SYS_ACTMSG_SYNC_ */
