
#include "pan_sys.h"


#ifndef DO_TIMINGS
#  define DO_TIMINGS
#endif

#include "pan_timer.h"


struct PAN_TIMER_T {
    int        ticks;
    pan_time_p total;
    pan_time_p start;
    pan_time_p stop;
};


pan_timer_p
pan_timer_create(void)
{
    pan_timer_p r;

    r = pan_malloc(sizeof(pan_timer_t));
    r->ticks = 0;
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
    pan_time_get(r->start);
}


void
pan_timer_stop(pan_timer_p r)
{
    pan_time_get(r->stop);
    pan_time_sub(r->stop, r->start);
    pan_time_add(r->total, r->stop);
    ++r->ticks;
}


int
pan_timer_read(pan_timer_p r, pan_time_p t)
{
    pan_time_copy(t, r->total);
    return r->ticks;
}


void
pan_timer_init(void)
{
}


void
pan_timer_end(void)
{
}
