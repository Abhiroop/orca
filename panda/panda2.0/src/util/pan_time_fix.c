/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <assert.h>
#include <math.h>
#include <limits.h>

#include "pan_util.h"

#include "pan_time_fix.h"

#ifdef NO_FIX_MACROS
#  define NANO 1000000000
#endif

pan_time_fix_t pan_time_fix_infinity	= {LONG_MAX, NANO};
pan_time_fix_t pan_time_fix_zero	= {0, 0};


/* Negative times:
 * invariant holds that time = tv_sec + NANO * t_usec., with 0 <= t_usec < NANO.
 * This differs from the conventional notation, for instance:
 * time = -0.25 is represented as { -1, 750000000 }
 *
 * The easiest way to negate a time value is by subtracting it from time_zero.
 */

void
pan_time_fix_start(void)
{
}
 
 
void
pan_time_fix_end(void)
{
}


#ifdef NO_FIX_MACROS

int
pan_time_fix_cmp(pan_time_fix_p t1, pan_time_fix_p t2)
{
    if (t1->t_sec == t2->t_sec) {
	if (t1->t_nsec == t2->t_nsec) {
	    return 0;
	} else if (t1->t_nsec > t2->t_nsec) {
	    return 1;
	} else {
	    return -1;
	}
    } else if (t1->t_sec > t2->t_sec) {
	return 1;
    } else {
	return -1;
    }
}


void pan_time_fix_add(pan_time_fix_p res, pan_time_fix_p delta)
{
    res->t_nsec += delta->t_nsec; /* max 2e9 = 2^31 */
    res->t_sec  += delta->t_sec;
    if (res->t_nsec >= NANO){
	res->t_nsec -= NANO;
	res->t_sec ++;
    }
}



void pan_time_fix_sub(pan_time_fix_p res, pan_time_fix_p delta)
{
    res->t_nsec -= delta->t_nsec; /* max 2e9 = 2^31 */
    res->t_sec  -= delta->t_sec;
    if (res->t_nsec < 0) {
	res->t_nsec += NANO;
	--res->t_sec;
    }
}

#endif	/* NO_FIX_MACROS */


void pan_time_fix_mul(pan_time_fix_p res, int nr)
{
    unsigned long int u;
    pan_time_fix_t    t1;

    u = labs(nr);
    t1 = pan_time_fix_zero;		/* Accumulate result in t1 */
    while (u != 0) {
	if ((u & 0x1) == 0x1) {
	    pan_time_fix_add(&t1, res);
	}
	u >>= 1;
	pan_time_fix_add(res, res);
    }
    if (nr < 0) {
	*res = pan_time_fix_zero;
	pan_time_fix_sub(res, &t1);
    } else {
	*res = t1;
    }
}



void pan_time_fix_div(pan_time_fix_p res, int nr)
{
    int            is_neg;
    pan_time_fix_t t1;

    is_neg = (nr < 0);
    if (is_neg) {
	nr = -nr;
    }
    if (pan_time_fix_cmp(res, &pan_time_fix_zero) < 0) {
					/* % operator ill-defined for negs */
	t1 = pan_time_fix_zero;
	pan_time_fix_sub(&t1, res);
	is_neg = !is_neg;
    } else {
	t1 = *res;
    }

    res->t_nsec = (t1.t_sec % nr) * (NANO / nr) + t1.t_nsec / nr;
    res->t_sec  = t1.t_sec / nr;
    if (res->t_nsec < 0) {
	assert(res->t_sec == 0);
	--res->t_sec;
	res->t_nsec = NANO + res->t_nsec;
    }

    if (is_neg) {
	t1 = pan_time_fix_zero;
	pan_time_fix_sub(&t1, res);
	*res = t1;
    }
}



void pan_time_fix_mulf(pan_time_fix_p res, double nr)
{
    pan_time_fix_t t_int;
    pan_time_fix_t t_frac;
    double         f;
    double         f_sec;
    double         f_nsec;
    int            is_neg;
    pan_time_fix_t t1;

    is_neg = (nr < 0);
    if (is_neg) {
	nr = -nr;
    }
    if (pan_time_fix_cmp(res, &pan_time_fix_zero) < 0) {
						/* modf is not OK for negs */
	t1 = pan_time_fix_zero;
	pan_time_fix_sub(&t1, res);
	is_neg = !is_neg;
    } else {
	t1 = *res;
    }

    f = t1.t_sec * nr;
    f_nsec = modf(f, &f_sec);
    t_int.t_sec   = (int) f_sec;
    t_int.t_nsec  = (int) (NANO * f_nsec);
    f = (t1.t_nsec * nr) / NANO;
    f_nsec = modf(f, &f_sec);
    t_frac.t_sec  = (int) f_sec;
    t_frac.t_nsec = (int) (NANO * f_nsec);
    *res = t_int;
    pan_time_fix_add(res, &t_frac);

    if (is_neg) {
	t1 = pan_time_fix_zero;
	pan_time_fix_sub(&t1, res);
	*res = t1;
    }
}


void
pan_time_fix_d2t(pan_time_fix_p t, double d)
{
    t->t_sec  = (long int)d;
    t->t_nsec = (long int)((d - t->t_sec) * NANO);
    if (t->t_nsec < 0) {
	t->t_sec  -= 1;
	t->t_nsec += NANO;
    }
}


double
pan_time_fix_t2d(pan_time_fix_p t)
{
    return t->t_nsec / ((double)NANO) + t->t_sec;
}
