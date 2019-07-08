#include "pan_sys.h"		/* Provides a system interface */
#include "pan_time.h"
#include "pan_error.h"

#include <sys/time.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>

pan_time_p pan_time_infinity;
pan_time_p pan_time_zero;


struct pan_time {
    struct timeval time;
};

typedef struct pan_time pan_time_t;


#ifdef __SVR4__
#  define SOLARIS2
#endif

#if (defined __svr4__) && (! defined SOLARIS2)
#  define SOLARIS2
#endif

#if (defined __SVR4) && (! defined SOLARIS2)
#  define SOLARIS2
#endif


#define NANO 1000000000
#define MICRO 1000000

#define FINE MICRO



/* Negative times:
 * invariant holds that time = tv_sec + FINE * t_usec., with 0 <= t_usec < FINE.
 * This differs from the conventional notation, for instance:
 * time = -0.25 is represented as { -1, 750000[000] }
 *
 * The easiest way to negate a time value is by subtracting it from time_zero.
 */


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
}


pan_time_p
pan_time_create(void)
{
    pan_time_p time;

    time = (pan_time_p)malloc(sizeof(struct pan_time));
    assert(time);

    return time;
}

void
pan_time_clear(pan_time_p time)
{
    free(time);
}


void
pan_time_copy(pan_time_p to, pan_time_p from)
{
    to->time = from->time;
}


void
pan_time_get(pan_time_p now)
{
#ifdef SOLARIS2
    int gettimeofday(struct timeval *tp);

    if (gettimeofday(&now->time) == -1){
        pan_panic("gettimeofday");
    }
#else
    int gettimeofday(struct timeval *tp, struct timezone *tz);

    if (gettimeofday(&now->time, NULL) == -1){
        pan_panic("gettimeofday");
    }
#endif
}

void
pan_time_set(pan_time_p time, long sec, unsigned long nsec)
{
    time->time.tv_sec  = sec;
#ifdef SOLARIS2
    time->time.tv_usec = nsec;
#else
    time->time.tv_usec = nsec / 1000;
#endif
}


int
pan_time_cmp(pan_time_p t1, pan_time_p t2)
{
    if (t1->time.tv_sec == t2->time.tv_sec) {
	if (t1->time.tv_usec == t2->time.tv_usec) {
	    return 0;
	} else if (t1->time.tv_usec > t2->time.tv_usec) {
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


void pan_time_add(pan_time_p res, pan_time_p delta)
{
    res->time.tv_usec += delta->time.tv_usec; /* max 2e9 = 2^31 */
    res->time.tv_sec  += delta->time.tv_sec;
    if (res->time.tv_usec >= FINE){
	res->time.tv_usec -= FINE;
	res->time.tv_sec ++;
    }
}



void pan_time_sub(pan_time_p res, pan_time_p delta)
{
    res->time.tv_usec -= delta->time.tv_usec; /* max 2e9 = 2^31 */
    res->time.tv_sec  -= delta->time.tv_sec;
    if (res->time.tv_usec < 0) {
	res->time.tv_usec += FINE;
	--res->time.tv_sec;
    }
}


void pan_time_mul(pan_time_p res, int nr)
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



void pan_time_div(pan_time_p res, int nr)
{
    boolean is_neg;
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

    res->time.tv_usec = (t1.time.tv_sec % nr) * (FINE / nr) +
			t1.time.tv_usec / nr;
    res->time.tv_sec  = t1.time.tv_sec / nr;
    if (res->time.tv_usec < 0) {
	assert(res->time.tv_sec == 0);
	--res->time.tv_sec;
	res->time.tv_usec = FINE + res->time.tv_usec;
    }

    if (is_neg) {
	pan_time_set(&t1, 0, 0);
	pan_time_sub(&t1, res);
	*res = t1;
    }
}



void pan_time_mulf(pan_time_p res, double nr)
{
    struct pan_time t_int;
    struct pan_time t_frac;
    double   f;
    double   f_sec;
    double   f_nsec;
    boolean  is_neg;
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
    t_int.time.tv_usec  = (int) (FINE * f_nsec);
    f = (t1.time.tv_usec * nr) / FINE;
    f_nsec = modf(f, &f_sec);
    t_frac.time.tv_sec  = (int) f_sec;
    t_frac.time.tv_usec = (int) (FINE * f_nsec);
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
    t->time.tv_usec = (long int)((d - t->time.tv_sec) * FINE);
    if (t->time.tv_usec < 0) {
	t->time.tv_sec  -= 1;
	t->time.tv_usec += FINE;
    }
}


double
pan_time_t2d(pan_time_p t)
{
    return t->time.tv_usec / ((double)FINE) + t->time.tv_sec;
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
    f->t_nsec = t->time.tv_usec;
}


int
pan_time_sub_fix_small(pan_time_p t1, pan_time_p t2, long int *d)
{
    pan_time_t     dt;

    dt = *t1;
    pan_time_sub(&dt, t2);
    if (dt.time.tv_sec == 0) {
	*d = dt.time.tv_usec;
	return 1;
    }
    return 0;
}
