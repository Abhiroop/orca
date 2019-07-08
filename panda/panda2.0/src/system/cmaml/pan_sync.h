#ifndef _SYS_ACTMSG_SYNC_
#define _SYS_ACTMSG_SYNC_

#include "pan_sys.h"

#define L_INIT   	0x1
#define L_TAKEN		0x2

struct pan_mutex {
    int		 l_state;
    pan_thread_p l_blocked;
    pan_thread_p l_blocked_tail;
    int          l_owner;  /* only valid if (l_state & (L_TAKEN | L_INIT)) */
};
    
#define C_INIT   	0x1
#define C_TAKEN		0x2

struct pan_cond {
    int		 c_state;
    pan_thread_p c_blocked;
    pan_thread_p c_blocked_tail;
    pan_mutex_p  c_lock;
};
    

/*
 * The following macros are for internal use in the system layer only.
 * The macros come in two flavours. When NDEBUG is set, we do not
 * register who owns a lock. Otherwise we do.
 */

#include "pan_threads.h"

#ifdef NDEBUG
#define pan_mutex_lock(lock) \
    do { \
        if (!((lock)->l_state & L_TAKEN)) { \
	    (lock)->l_state |= L_TAKEN; \
        } else { \
	    pan_lock_acquire((lock), pan_cur_thread); \
        } \
    } while(0)

#define pan_mutex_unlock(lock) \
    do { \
        if (!(lock)->l_blocked) { \
	    (lock)->l_state &= ~L_TAKEN; \
        } else { \
	    pan_thread_p t = (lock)->l_blocked; \
	    (lock)->l_blocked = t->t_next; \
	    t->t_next = 0; \
	    t->t_state |= T_RUNNABLE; \
	    if (t == pan_daemon) { \
   	        pan_thread_run_daemon(); \
	    } \
        } \
     } while(0)

#else /* !NDEBUG */

#define pan_mutex_lock(lock) \
    do { \
        if ((lock)->l_state & L_TAKEN) { \
            (lock)->l_state |= L_TAKEN; \
            (lock)->l_owner = pan_cur_thread - pan_all_thread; \
        } else { \
            pan_lock_acquire((lock), pan_cur_thread); \
	} \
    } while(0)

#define pan_mutex_unlock(lock) \
    do { \
        if (!(lock)->l_blocked) { \
	    (lock)->l_state &= ~L_TAKEN; \
	    (lock)->l_owner = -1; \
        } else { \
	    pan_thread_p t = (lock)->l_blocked; \
	    (lock)->l_blocked = t->t_next; \
   	    (lock)->l_owner = t - pan_all_thread; \
	    t->t_next = 0; \
	    t->t_state |= T_RUNNABLE; \
	    if (t == pan_daemon) { \
   	        pan_thread_run_daemon(); \
	    } \
        } \
     } while(0)
#endif

extern void pan_sys_sync_start(void);
extern void pan_sys_sync_end(void);

#endif /* _SYS_ACTMSG_SYNC_ */
