#ifndef __thrpool_h__
#define __thrpool_h__

#include "orca_types.h"


#define thrpool_put(tp, val, type) \
do { \
    tp_job_t *_myjob; \
\
    sys_mutex_lock(&(tp)->tp_lock); \
\
    if (!(tp)->tp_freelist) {		/* job queue full? */ \
	add_jobs(tp); \
    } \
\
    _myjob = (tp)->tp_freelist; \
    (tp)->tp_freelist = _myjob->tpj_next; \
\
    *(type *)(&_myjob->tpj_buf) = (val); \
\
    if ((tp)->tp_head == (tp_job_t *)0) { \
	(tp)->tp_head = _myjob; \
    } else { \
	(tp)->tp_tail->tpj_next = _myjob; \
    } \
    (tp)->tp_tail   = _myjob; \
    _myjob->tpj_next = (tp_job_t *)0; \
\
    if ( (tp)->tp_idle > 0) {			   /* some idle worker? */ \
    	sys_cond_signal(&(tp)->tp_cond); \
    } else if ((tp)->tp_workers < (tp)->tp_maxsize)	{  /* max workers in use? */ \
	add_workers(tp); \
    } \
\
    sys_mutex_unlock(&(tp)->tp_lock); \
} while(0)


extern void add_workers(thrpool_t *tp);   /* use in thrpool_put only */

extern void add_jobs(thrpool_t *tp);   /* use in thrpool_put only */

extern void thrpool_init(thrpool_t *tp, thrpool_func_p upcall, int jobsize,
			 int maxsize, int increment, int prio);

extern void thrpool_clear(thrpool_t *tp);

#endif
