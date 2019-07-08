#ifndef DO_TIMINGS
#  define DO_TIMINGS
#endif

#include <stdio.h>

#include "pan_sys_msg.h"

#include "pan_system.h"

#include "pan_time.h"
#include "pan_timer.h"


#ifdef pan_timer_start
#  undef pan_timer_start
#endif
#ifdef pan_timer_stop
#  undef pan_timer_stop
#endif



pan_timer_p
pan_timer_create(void)
{
    pan_timer_p r;

    r = pan_malloc(sizeof(pan_timer_t));
    r->starts = 0;
    r->stops  = 0;
    r->total = pan_time_create();
    r->start = pan_time_create();
    r->stop  = pan_time_create();
    pan_time_d2t(r->total, 0.0);

    return r;
}


void
pan_timer_clear(pan_timer_p r)
{
    pan_time_clear(r->total);
    pan_time_clear(r->start);
    pan_time_clear(r->stop);
    pan_free(r);
}


void
pan_timer_start(pan_timer_p r)
{
    if (r->starts == r->stops) {
	++r->starts;
    }
    pan_time_get(r->start);
}


void
pan_timer_stop(pan_timer_p r)
{
    if (r->stops == r->starts - 1) {
	++r->stops;
	pan_time_get(r->stop);
	pan_time_sub(r->stop, r->start);
	pan_time_add(r->total, r->stop);
    }
}


int
pan_timer_read(pan_timer_p r, pan_time_p t)
{
    pan_time_copy(t, r->total);
    return r->stops;
}


void
pan_timer_print(pan_timer_p r, char *label)
{
    int n;
    pan_time_p t = pan_time_create();

    n = pan_timer_read(r, t);
    if (n > 0) {
	printf("%2d: Latency: %-16s total = %10.6f s ",
		pan_my_pid(), label, pan_time_t2d(t));
	pan_time_div(t, n);
	printf("av. = %8.6f s N = %5d\n", pan_time_t2d(t), n);
    }
}


void
pan_timer_init(void)
{
}


void
pan_timer_end(void)
{
}
