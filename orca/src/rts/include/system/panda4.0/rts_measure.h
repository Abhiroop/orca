/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __rts_measure_h__
#define __rts_measure_h__

#include "pan_sys.h"

typedef struct rts_measure {
    char         *rt_name;       
    unsigned long rt_ticks;
    pan_time_p    rt_start;
    pan_time_p    rt_stop;
    pan_time_p    rt_sum;
} rts_timer_t, *rts_timer_p;


#ifdef RTS_MEASURE

#define rts_measure_enter(t)  pan_time_get((t)->rt_start)

#define rts_measure_leave(t) \
  do { \
	 pan_time_get((t)->rt_stop); \
	 pan_time_sub((t)->rt_stop, (t)->rt_start); \
	 pan_time_add((t)->rt_sum, (t)->rt_stop); \
	 (t)->rt_ticks++; \
  } while(0)

extern void rts_measure_start(void);

extern void rts_measure_stop(void);

extern rts_timer_p rts_measure_create(char *name);

extern void rts_measure_dump(void);

#else

#define rts_measure_start()

#define rts_measure_end()

#define rts_measure_enter(t)

#define rts_measure_leave(t)

#define rts_measure_create(name)  ( (rts_timer_p)0 )

#define rts_measure_dump()

#endif /* RTS_MEASURE */

#endif
