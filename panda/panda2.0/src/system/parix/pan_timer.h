#ifndef __SYS_PAN_TIMER_H__
#define __SYS_PAN_TIMER_H__

#include "pan_sys.h"


typedef struct PAN_TIMER_T pan_timer_t, *pan_timer_p;

#ifdef DO_TIMINGS

pan_timer_p pan_timer_create(void);
void        pan_timer_clear(pan_timer_p r);

void        pan_timer_start(pan_timer_p r);
void        pan_timer_stop(pan_timer_p r);

int         pan_timer_read(pan_timer_p r, pan_time_p t);

void        pan_timer_init(void);
void        pan_timer_end(void);

#else

#define pan_timer_create()	NULL
#define pan_timer_clear(r)

#define pan_timer_start(r)
#define pan_timer_stop(r)

#define pan_timer_read(r, t)	-1

#define pan_timer_init()
#define pan_timer_end()

#endif

#endif
