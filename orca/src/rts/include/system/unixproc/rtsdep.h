/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: rtsdep.h,v 1.31 1999/08/16 13:58:49 ceriel Exp $ */

/* Version for Unix process. */

#ifndef SOLARIS2
#ifdef __svr4__
#define SOLARIS2
#endif

#ifdef __SVR4__
#define SOLARIS2
#endif

#ifdef __SVR4
#define SOLARIS2
#endif
#endif

#ifdef SOLARIS2
#include <thread.h>

#define pthread_mutex_t	mutex_t
#define pthread_cond_t	cond_t
#define pthread_t	thread_t
#define pthread_key_t	thread_key_t

#define pthread_init()
#define pthread_yield(x) \
			thr_yield()
#define pthread_join(x, y) \
			thr_join(x, NULL, y)
#define pthread_create(a, b, c, d) \
			(thr_create(b, 0, c, d, THR_BOUND, a) != 0 ? -1 : 0)
#define pthread_mutex_init(m, x) \
			(mutex_init(m, USYNC_THREAD, x) != 0 ? -1 : 0)
#define pthread_mutex_destroy(m) \
			mutex_destroy(m)
#define pthread_mutex_lock(m) \
			mutex_lock(m)
#define pthread_mutex_unlock(m) \
			mutex_unlock(m)
#define pthread_cond_init(c, x) \
			(cond_init(c, USYNC_THREAD, x) != 0 ? -1 : 0)
#define pthread_cond_destroy(c) \
			cond_destroy(c)
#define pthread_cond_wait(c, l) \
			cond_wait(c, l)
#define pthread_cond_broadcast(c) \
			cond_broadcast(c)
#define pthread_cond_signal(c) \
			cond_signal(c)
#define pthread_key_create(k, x) \
			(thr_keycreate(k, x) != 0 ? -1 : 0)
#define pthread_getspecific(k, x) \
			thr_getspecific(k, x)
#define pthread_setspecific(k, x) \
			thr_setspecific(k, x)
#elif defined(__bsdi__) || defined(LINUX) || defined(__linux)
#include <pthread.h>
#else
#include "threads/pthread.h"
#endif

struct t_objrts {
	pthread_mutex_t		oo_mutex;
	pthread_cond_t		oo_cond;
	int			oo_procsblocking;
	int			oo_shared;
	int			oo_refcount;
};

#define o_mutex			o_rtsdep->oo_mutex
#define o_cond			o_rtsdep->oo_cond
#define o_procsblocking		o_rtsdep->oo_procsblocking
#define o_refcount		o_rtsdep->oo_refcount
#define o_shared		o_rtsdep->oo_shared

#define SCHEDINIT	32768

extern int schedcount;
extern int ncpus;

#if defined(SOLARIS2)
#define m_rts()
#elif defined(__bsdi__) || defined(__linux)
#define m_rts()		if (--schedcount == 0) \
				(schedcount = SCHEDINIT), sched_yield()
#else
#define m_rts()		if (--schedcount == 0) \
				(schedcount = SCHEDINIT), pthread_yield(NULL)
#endif

#define o_init_rtsdep(o, d, s) \
			((o)->o_rtsdep = m_malloc(sizeof(struct t_objrts))), \
			pthread_mutex_init(&(o)->o_mutex, NULL), \
			pthread_cond_init(&(o)->o_cond, NULL), \
			((o)->o_shared = 0), \
			((o)->o_refcount = 1), \
			((o)->o_procsblocking = 0)

#define o_kill_rtsdep(o) \
			pthread_mutex_destroy(&(o)->o_mutex), \
			pthread_cond_destroy(&(o)->o_cond), \
			m_free((o)->o_rtsdep)

#define o_isshared(o)	((o)->o_shared != 0)

#define m_ncpus()	(ncpus)

#define m_flush(f)	fflush(f)


#define m_objdescr_reg(p, n, s)

#define __Score(a,x,b,c,d)
#define __erocS(a,x,b,c,d)

#define m_strategy(a, b, c)

/* No marshalling. */
#define o_rts_nbytes(p, d) 0
#define o_rts_marshall(p, data, d) \
                              p
#define o_rts_unmarshall(p, data, d) \
                              p
