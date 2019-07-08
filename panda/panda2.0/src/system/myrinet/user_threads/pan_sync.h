#ifndef _SYS_ACTMSG_SYNC_
#define _SYS_ACTMSG_SYNC_

#include "pan_sys_msg.h"

#include "pan_threads.h"


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
    
extern void pan_sys_sync_start(void);
extern void pan_sys_sync_end(void);

extern void pan_mutex_init(pan_mutex_p lock);
extern void pan_lock_acquire_internal(pan_mutex_p lock, pan_thread_p t);
extern void pan_mutex_do_unlock(pan_mutex_p lock);


#ifndef NO_SYNC_MACROS


#ifdef LOCK_WRITE_OWNER

#define pan_mutex_lock(lck) \
    do { \
	int *__p_taken = &((lck)->l_taken); \
	if (! *__p_taken) { \
	    *__p_taken = 1; \
	    (lck)->l_owner = pan_cur_thread; \
	} else { \
	    pan_lock_acquire_internal(lck, pan_cur_thread); \
	} \
    } while (0)

#  define pan_mutex_unlock(lck) \
    do { \
	assert((lck)->l_inited); \
	assert((lck)->l_taken); \
	assert((lck)->l_owner == pan_cur_thread); \
	if ((lck)->l_blocked) { \
	    pan_thread_p __t = (lck)->l_blocked; \
	    assert((lck)->l_inited); \
	    assert((lck)->l_taken); \
	    assert((lck)->l_owner == pan_cur_thread); \
	    assert((lck)->l_blocked); \
	    (lck)->l_blocked = __t->t_next; \
	    (lck)->l_owner = __t; \
	    __t->t_next = 0; \
	    __t->t_state |= T_RUNNABLE; \
	} else { \
	    (lck)->l_taken = 0; \
	    (lck)->l_owner = NULL; \
	} \
    } while (0)

#else		/* LOCK_WRITE_OWNER */

#  define pan_mutex_lock(lck) \
    do { \
	int *__p_taken = &((lck)->l_taken); \
	if (! *__p_taken) { \
	    *__p_taken = 1; \
	} else { \
	    pan_lock_acquire_internal(lck, pan_cur_thread); \
	} \
    } while (0)

#  define pan_mutex_unlock(lck) \
    do { \
	if ((lck)->l_blocked) { \
	    pan_thread_p __t = (lck)->l_blocked; \
	    assert((lck)->l_taken); \
	    assert((lck)->l_blocked); \
	    (lck)->l_blocked = __t->t_next; \
	    __t->t_next = 0; \
	    __t->t_state |= T_RUNNABLE; \
	} else { \
	    (lck)->l_taken = 0; \
	} \
    } while (0)

#endif		/* LOCK_WRITE_OWNER */

#endif		/* NO_SYNC_MACROS */

#endif /* _SYS_ACTMSG_SYNC_ */
