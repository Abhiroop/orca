/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __RTSDEP_H__
#define __RTSDEP_H__

#include "rts_types.h"

/* An object fragment's RTS-dependent part consists of the object's
 * type and an RTS object. 
 */
struct t_objrts {
    tp_dscr      *type;  /* object's type descriptor */
    rts_object_t  obj;   /* RTS part of object */
};

#define DONT_LOCK_RTS       0
#define LOCK_RTS            1

/* These includes after type specification so inlining works */
#include "process.h"
#include "fragment.h"
#include "pan_trace.h"

extern int rts_base_pid;

#define m_ncpus()	(rts_nr_platforms - rts_base_pid)
#define m_mycpu()	(rts_my_pid - rts_base_pid)

#ifdef RTS_POLL

extern int rts_poll_count, rts_max_poll, rts_slice_count, rts_max_slice,
	   rts_step_count;
extern void pan_poll(void);

#define m_rts()		do { \
			    if (--rts_step_count <= 0) { \
				if (rts_poll_count <= 0) { \
				    pan_poll(); \
				    rts_poll_count = rts_max_poll; \
				} \
				if (rts_slice_count <= 0) { \
				    pan_thread_yield(); \
				    rts_slice_count = rts_max_slice; \
				} \
				rts_step_count = rts_poll_count; \
				if (rts_slice_count < rts_step_count) { \
				    rts_step_count = rts_slice_count; \
				} \
				rts_slice_count -= rts_step_count; \
				rts_poll_count -= rts_step_count; \
			    } \
			} while(0)
#else
#define m_rts()
#endif

#define o_start_read(o)          f_start_read( (o) )
#define o_end_read(o)            rts_unlock()
#define o_start_write(o)         f_start_write( (o) )
#define o_end_write(o, res)      f_end_write( (o) )

#define o_isshared(o)		 ( f_get_status( o) != f_unshared)

#define o_init_rtsdep(o, d, s)\
		(f_init( (fragment_p)(o), (d), (s)), \
		 f_trc_create( (fragment_p)(o), ((fragment_p)(o))->fr_name))
#define o_kill_rtsdep(o)		f_clear( (fragment_p)o)

#include "pan_sys.h"

#define rts_lock()          pan_mutex_lock(rts_global_lock)
#define rts_trylock()       pan_mutex_trylock(rts_global_lock)
#define rts_unlock()        pan_mutex_unlock(rts_global_lock)
 
#define rts_cond_create()   pan_cond_create(rts_global_lock)
#define rts_cond_clear(c)   pan_cond_clear(c)
  
extern pan_mutex_p rts_global_lock;    /* The global RTS lock */
extern pan_mutex_p rts_read_lock;
extern pan_mutex_p rts_write_lock;



#define __Score(data, td, score, naccess, uncertainty) \
      rts_score(data, td, score, naccess, uncertainty, LOCK_RTS)
 
#define __erocS(data, td, score, naccess, uncertainty) \
      rts_erocs(data, td, score, naccess, uncertainty, LOCK_RTS)

#define m_malloc(sz)	      (pan_malloc(sz))
#define m_realloc(ptr, sz)    (pan_realloc(ptr, sz))
#ifndef m_free
#define m_free(ptr)	      do { void *_p = (ptr); if (_p) pan_free(_p);} while (0)
#endif

#define o_rts_nbytes(o,d)       (0)
#define o_rts_marshall(p,o,d)   (p)
#define o_rts_unmarshall(p,o,d) (f_init((fragment_p) o, d, "by-value copy"), (p))

#endif
