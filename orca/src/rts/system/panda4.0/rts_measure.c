/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifdef RTS_MEASURE

#include <stdio.h>
#include "pan_sys.h"
#include "rts_measure.h"
#include "rts_types.h"

#define RTS_MEASURE_EXTEND  16

static int timers_used, max_timers;
static rts_timer_p timers;

static void
add_timers(int first, int n)
{
    int i;

    for (i = first; i < first + n; i++) {
	timers[i].rt_name  = 0;
	timers[i].rt_ticks = 0;
	timers[i].rt_start = pan_time_create();
	timers[i].rt_stop  = pan_time_create();
	timers[i].rt_sum   = pan_time_create();
    }
}


void
rts_measure_start(void)
{
    timers_used = 0;
    max_timers  = RTS_MEASURE_EXTEND;
    timers = (rts_timer_p)m_malloc(max_timers * sizeof(rts_timer_t));
    add_timers(0, max_timers);
}


void
rts_measure_end(void)
{
    int i;

    for (i = 0; i < max_timers; i++) {
	timers[i].rt_name  = 0;
	timers[i].rt_ticks = 0;

	pan_time_clear(timers[i].rt_start);
	pan_time_clear(timers[i].rt_stop);
	pan_time_clear(timers[i].rt_sum);
	
	timers[i].rt_start = 0;
	timers[i].rt_stop  = 0;
	timers[i].rt_sum   = 0;
    }

    m_free(timers);
    timers = 0;
    max_timers = 0;
}    


rts_timer_p
rts_measure_create(char *name)
{
    rts_timer_p t;
    int ntimers;

    if (++timers_used == max_timers) {
	ntimers = max_timers;
	max_timers += RTS_MEASURE_EXTEND;
	timers = m_realloc(timers, max_timers * sizeof(rts_timer_t));
	add_timers(ntimers, RTS_MEASURE_EXTEND);
    }

    t = &timers[timers_used - 1];
    t->rt_name = name;
    return t;
}


void
rts_measure_dump(void)
{
    rts_timer_p t;
    int i;

    for (i = 0; i < timers_used; i++) {
	t = &timers[i];
	if (t->rt_ticks == 0) {
	    continue;
	}
	printf("%2d) %20s %1f usec %u ticks\n",
	       rts_my_pid,
	       t->rt_name,
	       pan_time_t2d(t->rt_sum) / t->rt_ticks * 1000000.0,
	       t->rt_ticks);
    }
}

#endif
