#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_time.h"
#include "pan_error.h"

#include <math.h>
#include <limits.h>
#ifndef __linux__               /* erik: linux has no sleep.h */
#include <sleep.h>
#endif
#include <time.h>

pan_time_p pan_time_infinity;
pan_time_p pan_time_zero;


/* Negative times:
 * invariant holds that time = tv_sec + NANO * t_usec., with 0 <= t_usec < NANO.
 * This differs from the conventional notation, for instance:
 * time = -0.25 is represented as { -1, 750000000 }
 *
 * The easiest way to negate a time value is by subtracting it from time_zero.
 */

#define NANO 1000000000

void
pan_sys_time_start(void)
{
    pan_time_infinity = pan_time_create();
    pan_time_set(pan_time_infinity, LONG_MAX, NANO);
    pan_time_zero = pan_time_create();
    pan_time_set(pan_time_zero, 0, 0);
}
 
 
void
pan_sys_time_end(void)
{
    pan_time_clear(pan_time_infinity);
    pan_time_clear(pan_time_zero);
}


pan_time_p
pan_time_create(void)
{
    pan_time_p time;

    time = (pan_time_p)pan_malloc(sizeof(struct pan_time));

    return time;
}

void
pan_time_clear(pan_time_p time)
{
    pan_free(time);
}


void
pan_time_copy(pan_time_p to, pan_time_p from)
{
    to->time = from->time;
}


void
pan_time_get(pan_time_p now)
{
    struct timeval t;

    gettimeofday(&t, DST_NONE);

    now->time.tv_sec = t.tv_sec;
    now->time.tv_nsec = t.tv_usec * 1000;
}

void
pan_time_set(pan_time_p time, long sec, unsigned long nsec)
{
    time->time.tv_sec  = sec;
    time->time.tv_nsec = nsec;
}


int
pan_time_cmp(pan_time_p t1, pan_time_p t2)
{
    if (t1->time.tv_sec == t2->time.tv_sec) {
	if (t1->time.tv_nsec == t2->time.tv_nsec) {
	    return 0;
	} else if (t1->time.tv_nsec > t2->time.tv_nsec) {
	    return 1;
	} else {
	    return -1;
	}
    } else if (t1->time.tv_sec > t2->time.tv_sec) {
	return 1;
    } else {
	return -1;
    }
}


void 
pan_time_add(pan_time_p res, pan_time_p delta)
{
    res->time.tv_nsec += delta->time.tv_nsec; /* max 2e9 = 2^31 */
    res->time.tv_sec  += delta->time.tv_sec;
    if (res->time.tv_nsec >= NANO){
	res->time.tv_nsec -= NANO;
	res->time.tv_sec ++;
    }
}



void 
pan_time_sub(pan_time_p res, pan_time_p delta)
{
    res->time.tv_nsec -= delta->time.tv_nsec; /* max 2e9 = 2^31 */
    res->time.tv_sec  -= delta->time.tv_sec;
    if (res->time.tv_nsec < 0) {
	res->time.tv_nsec += NANO;
	--res->time.tv_sec;
    }
}


void 
pan_time_mul(pan_time_p res, int nr)
{
    unsigned long int u;
    struct pan_time   t1;

    u = labs(nr);
    pan_time_set(&t1, 0, 0);		/* Accumulate result in t1 */
    while (u != 0) {
	if ((u & 0x1) == 0x1) {
	    pan_time_add(&t1, res);
	}
	u >>= 1;
	pan_time_add(res, res);
    }
    if (nr < 0) {
	pan_time_set(res, 0, 0);
	pan_time_sub(res, &t1);
    } else {
	*res = t1;
    }
}



void 
pan_time_div(pan_time_p res, int nr)
{
    int is_neg;
    struct pan_time t1;

    is_neg = (nr < 0);
    if (is_neg) {
	nr = -nr;
    }
    if (pan_time_cmp(res, pan_time_zero) < 0) {
					/* % operator ill-defined for negs */
	pan_time_set(&t1, 0, 0);
	pan_time_sub(&t1, res);
	is_neg = !is_neg;
    } else {
	t1 = *res;
    }

    res->time.tv_nsec = (t1.time.tv_sec % nr) * (NANO / nr) +
			t1.time.tv_nsec / nr;
    res->time.tv_sec  = t1.time.tv_sec / nr;
    if (res->time.tv_nsec < 0) {
	assert(res->time.tv_sec == 0);
	--res->time.tv_sec;
	res->time.tv_nsec = NANO + res->time.tv_nsec;
    }

    if (is_neg) {
	pan_time_set(&t1, 0, 0);
	pan_time_sub(&t1, res);
	*res = t1;
    }
}



void 
pan_time_mulf(pan_time_p res, double nr)
{
    struct pan_time t_int;
    struct pan_time t_frac;
    double   f;
    double   f_sec;
    double   f_nsec;
    int      is_neg;
    struct pan_time t1;

    is_neg = (nr < 0);
    if (is_neg) {
	nr = -nr;
    }
    if (pan_time_cmp(res, pan_time_zero) < 0) {
						/* modf is not OK for negs */
	pan_time_set(&t1, 0, 0);
	pan_time_sub(&t1, res);
	is_neg = !is_neg;
    } else {
	t1 = *res;
    }

    f = t1.time.tv_sec * nr;
    f_nsec = modf(f, &f_sec);
    t_int.time.tv_sec   = (int) f_sec;
    t_int.time.tv_nsec  = (int) (NANO * f_nsec);
    f = (t1.time.tv_nsec * nr) / NANO;
    f_nsec = modf(f, &f_sec);
    t_frac.time.tv_sec  = (int) f_sec;
    t_frac.time.tv_nsec = (int) (NANO * f_nsec);
    *res = t_int;
    pan_time_add(res, &t_frac);

    if (is_neg) {
	pan_time_set(&t1, 0, 0);
	pan_time_sub(&t1, res);
	*res = t1;
    }
}


void
pan_time_d2t(pan_time_p t, double d)
{
    t->time.tv_sec  = (long int)d;
    t->time.tv_nsec = (long int)((d - t->time.tv_sec) * NANO);
    if (t->time.tv_nsec < 0) {
	t->time.tv_sec  -= 1;
	t->time.tv_nsec += NANO;
    }
}


double
pan_time_t2d(pan_time_p t)
{
    return t->time.tv_nsec / ((double)NANO) + t->time.tv_sec;
}


int
pan_time_size(void)
{
    return sizeof(struct pan_time);
}


void
pan_time_marshall(pan_time_p p, void *buffer)
{
    pan_time_p t_buffer = buffer;

    *t_buffer = *p;
}


void
pan_time_unmarshall(pan_time_p p, void *buffer)
{
    pan_time_p t_buffer = buffer;

    *p = *t_buffer;
}


void
pan_time_t2fix(pan_time_p t, pan_time_fix_p f)
{
    f->t_sec = t->time.tv_sec;
    f->t_nsec = t->time.tv_nsec;
}


int
pan_time_sub_fix_small(pan_time_p t1, pan_time_p t2, long int *d)
{
    pan_time_t     dt;

    dt = *t1;
    pan_time_sub(&dt, t2);
    if (dt.time.tv_sec == 0) {
	*d = dt.time.tv_nsec;
	return 1;
    }
    return 0;
}
