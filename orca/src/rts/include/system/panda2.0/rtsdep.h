/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __RTSDEP_H__
#define __RTSDEP_H__

#include "rts_types.h"
#include "process.h"
#include "fragment.h"
#include "pan_trace.h"


/* An object fragment's RTS-dependent part consists of the object's
 * type and an RTS object. 
 */
struct t_objrts {
    tp_dscr      *type;  /* object's type descriptor */
    rts_object_t  obj;   /* RTS part of object */
};


extern int rts_base_pid;

#define m_ncpus()	(rts_nr_platforms - rts_base_pid)
#define m_mycpu()	(rts_my_pid - rts_base_pid)

#ifdef RTS_POLL
extern int rts_poll_count, rts_max_poll;
extern void pan_poll(void);

#define m_rts()      do { \
                         if (++rts_poll_count >= rts_max_poll) { \
                             rts_poll_count = 0; \
			     pan_poll(); \
			     pan_thread_yield(); \
			 } \
                     } while(0);
#else
#define m_rts()
#endif

#define m_flush(f)		fflush(f)

#define o_start_read(o)          f_start_read( (o) )
#define o_end_read(o)            f_end_read( (o) )
#define o_start_write(o)         f_start_write( (o) )
#define o_end_write(o, res)      f_end_write( (o) )

#define o_init_rtsdep(o, d, s)\
		(f_init( (fragment_p)(o), (d), (s)), \
		 f_trc_create( (fragment_p)(o), ((fragment_p)(o))->fr_name))
#define o_kill_rtsdep(o)		f_clear( (fragment_p)o)

#define o_isshared(o)		 ( f_get_status( o) != f_unshared)

#define m_malloc(sz)	      (pan_malloc(sz))
#define m_realloc(ptr, sz)    (pan_realloc(ptr, sz))
#ifndef m_free
#define m_free(ptr)	      do { void *_p = (ptr); if (_p) pan_free(_p);} while (0)
#endif

#define o_rts_nbytes(o,d)	(0)
#define o_rts_marshall(p,o,d)	(p)
#define o_rts_unmarshall(p,o,d) (f_init((fragment_p) o, d, "by-value copy"), (p))

#endif
