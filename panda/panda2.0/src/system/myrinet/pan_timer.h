#ifndef __SYS_PAN_TIMER_H__
#define __SYS_PAN_TIMER_H__

/*
#include "pan_sys_msg.h"
*/

#include "pan_time.h"


typedef struct PAN_TIMER_T pan_timer_t, *pan_timer_p;

#ifdef DO_TIMINGS

struct PAN_TIMER_T {
    int        stops;
    int        starts;
    pan_time_p total;
    pan_time_p start;
    pan_time_p stop;
};

pan_timer_p pan_timer_create(void);
void        pan_timer_clear(pan_timer_p r);

void        pan_timer_start(pan_timer_p r);
void        pan_timer_stop(pan_timer_p r);

#ifndef USE_TIMER_FUNCTIONS

#define pan_timer_start(r) \
	do { \
	    if ((r)->starts == (r)->stops) \
		++(r)->starts; \
	    pan_time_get((r)->start); \
	} while (0)

#define pan_timer_stop(r) \
	do { \
	    if ((r)->stops == (r)->starts - 1) { \
		++(r)->stops; \
		pan_time_get((r)->stop); \
		pan_time_sub((r)->stop, (r)->start); \
		pan_time_add((r)->total, (r)->stop); \
	    } \
	} while (0)

#endif


int         pan_timer_read(pan_timer_p r, pan_time_p t);

void        pan_timer_print(pan_timer_p r, char *label);

void        pan_timer_init(void);
void        pan_timer_end(void);

#else

#define pan_timer_create()	NULL
#define pan_timer_clear(r)

#define pan_timer_start(r)
#define pan_timer_stop(r)

#define pan_timer_read(r, t)	-1

#define pan_timer_print(r, label)

#define pan_timer_init()
#define pan_timer_end()

#endif

#endif
