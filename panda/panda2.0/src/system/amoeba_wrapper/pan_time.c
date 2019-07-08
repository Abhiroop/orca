#include <amoeba.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <module/host.h>
#include <module/direct.h>
#include <module/syscall.h>
#include <proc.h>
#include <stderr.h>
#include <limits.h>
#include <string.h>

#include <machdep/dev/sun4m_timer.h>


#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_time.h"
#include "pan_error.h"

#include <math.h>


#ifndef NANO
#  define NANO 1000000000
#endif

pan_time_p pan_time_infinity;
pan_time_p pan_time_zero;


const unsigned long int bit_31 = (0x1 << 31);
const double two_32 = 4294967296.0;


#ifndef NO_MAPPED_TIMER

#define bit_30_31	(0x3 << 30)


static pc_timer_type *mapped_timer;



/* The mapped micro-second timer is a 64-bit word. It is fed with 2 MHz at
 * bit 9, so bit 0-8 have no meaning.
 * Nobody is interested in the following, but who cares:
 * Since 2^21 more or less resembles 2e06, bits 30-63 contain the seconds
 * since clock start, off by a factor 2^21/2e06 = 1.024^2.
 */

/* Time is represented as a long long int. For manipulation, there is also
 * union type which yields access to the higher and lower 32 bits.
 * The sign bit is in the higher field.
 * RFHH */



#if defined __GNUC__ && ! defined NO_LONGLONG

struct longiu {
    long int           hi;
    unsigned long int  lo;
};

typedef union {
    long_long          longlong;
    struct longiu      longs;
} double_long;


void
amoeba_gettime(pan_time_p now)
{
					/* gcc does not handle *& well */
    /* copy_timer(mapped_timer, (union sun4m_timer *)now); */

    *(union sun4m_timer*)now = *mapped_timer;
}



void
pan_sys_time_start(void)
{
    struct sun4m_timer_regs *mapped_timer_struct;

    sun4m_timer_init(NULL);
    mapped_timer_struct = sun4m_timer_getptr();
    mapped_timer = &mapped_timer_struct->pc_timer;

    pan_time_infinity = pan_time_create();
    ((double_long*)pan_time_infinity)->longs.hi = LONG_MAX;
    ((double_long*)pan_time_infinity)->longs.lo = ULONG_MAX;
    pan_time_zero = pan_time_create();
    pan_time_zero->t = 0;
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

    time = (pan_time_p)pan_malloc(sizeof(pan_time_t));
    memset(time, 0, sizeof(pan_time_t));

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
    *to = *from;
}


void
pan_time_get(pan_time_p now)
{
    amoeba_gettime(now);
}

void
pan_time_set(pan_time_p time, long sec, unsigned long nsec)
{
    double d;

    d = sec + ((double)nsec) / NANO;
    pan_time_d2t(time, d);
}

int
pan_time_cmp(pan_time_p t1, pan_time_p t2)
{
    if (t1->t == t2->t) {
	return 0;
    } else if (t1->t < t2->t){
	return -1;
    } else {
	return 1;
    }
}


void 
pan_time_add(pan_time_p res, pan_time_p delta)
{
    res->t += delta->t;
}



void 
pan_time_sub(pan_time_p res, pan_time_p delta)
{
    res->t -= delta->t;
}


void 
pan_time_mul(pan_time_p res, int nr)
{
    res->t *= nr;
}



void 
pan_time_div(pan_time_p res, int nr)
{
    res->t /= nr;
}



void 
pan_time_mulf(pan_time_p res, double nr)
{
    res->t *= (long_long)nr;
}


void
pan_time_d2t(pan_time_p t, double d)
{
    t->t = (long_long)(d * 1.024E09);
}


double
pan_time_t2d(pan_time_p t)
{
    return t->t * 0.9765625E-09;
}


#ifdef PAN_TIME_FIX_DBL


void
pan_time_t2fix(pan_time_p t, pan_time_fix_p f)
{
    double dsecs;

    *(pan_time_p)f = *t;

    f->t_sec <<= 2;
    f->t_sec |= ((long unsigned int)(f->t_nsec) & bit_30_31) >> 30;
    (long unsigned int)(f->t_nsec) &= ~bit_30_31;

    dsecs  = f->t_sec * (1.024 * 1.024);
    f->t_sec = (long int)dsecs;
    f->t_nsec = (long int)(NANO * (dsecs - f->t_sec)) +
			(long int)(f->t_nsec * 0.9765625);
}


#elif defined PAN_TIME_FIX_T2D


void
pan_time_t2fix(pan_time_p t, pan_time_fix_p f)
{
    double d;

    d = pan_time_t2d(t);
    f->t_sec = (long int)d;
    f->t_nsec = (long int)(NANO * (d - f->t_sec));
}


#else


void
pan_time_t2fix(pan_time_p t, pan_time_fix_p f)
{
    double_long       time;
    long unsigned int nanos;

    time.longs = *(struct longiu *)t;

			/* Correct 1K <--> 1000; factor 1.024 ^ 2 = 1.048576
			 * correct by steps:
			 * factor 1 + 1/2^5, yields 1.03125; then
			 * factor 1 + 1/2^6, yields 1.047363281250; then
			 * factor 1 + 1/2^10, yields 1.048386096954; then
			 * factor 1 + 1/2^13, yields 1.048514073771; then
			 * factor 1 + 1/2^14, yields 1.048578069991;
			 * factor 1 - 1/2^19, yields 1.048576069987;
			 * we are off now by a factor 1.000000066744,
			 * which seems sufficient. */

    time.longlong += (time.longlong >> 5);
    time.longlong += (time.longlong >> 6);
    time.longlong += (time.longlong >> 10);
    time.longlong += (time.longlong >> 13);
    time.longlong += (time.longlong >> 14);
    time.longlong -= (time.longlong >> 19);

    f->t_sec = (time.longs.hi << 2) | (time.longs.lo & bit_30_31) >> 30;

			/* Bits 0 .. 30 are seconds below the decimal point.
			 * Scale them so all 1s becomes 999999999 nanos:
			 * divide by 1.024 ^ 3.
			 * The "division" below is off by a factor
			 * 1.000000001143 */
    nanos = time.longs.lo & ~bit_30_31;
    nanos -= nanos >> 4;
    nanos -= nanos >> 7;
    nanos += nanos >> 10;
    nanos += nanos >> 12;
    nanos += nanos >> 16;
    nanos -= nanos >> 18;
    nanos += nanos >> 18;

    f->t_nsec = nanos;
}


#endif



int
pan_time_sub_fix_small(pan_time_p t1, pan_time_p t2, long int *d)
{
    long_long lld;

    lld = (t1->t - t2->t) * 0.9765625;
    if (lld < NANO && lld >= 0) {
	*d = lld;
	return 1;
    }
    return 0;
}


#else		/* __GNUC__ */






/* Negative times: 
 * 
 * invariant holds that time = tv_sec + NANO *
 *                             t_usec., with 0 <= t_usec < NANO.
 *
 * This differs from the conventional notation, for instance:
 * time = -0.25 is represented as { -1, 750000000 }.
 *
 * The easiest way to negate a time is by subtracting it from time_zero.
 */


/* Conversion:
 * microsecond timer is fed 2 MHz in bit 9, so its value n corresponds to
 * n / 1.024 nanoseconds (microseconds in bit 10; unused bits 0-9 correspond
 * to factor 1024).
 * Now, split ticks into bits 0-29 and bits 30-61.
 * hi = ent( n / 2^30);
 * lo = n % 2^30
 * n ticks = hi . 2 ^ 30 + lo ticks =
 *         = hi . 2 ^ 30 / 1.024 + lo / 1.024 nanoseconds =
 *         = hi . 2 ^ 30 / 1.024 . 10^-9 s + lo / 1.024 ns =
 *         = ent( hi . 1.024^2 ) s + fmod( hi . 1.024^2, 1) s + lo / 1.024 ns
 *         = ent( hi . 1.024^2 ) s + 10^9 fmod( hi . 1.024^2, 1) + lo / 1.024 ns
 *
 * So n ticks corresponds to:
 *   sec = ent( hi . 1.024^2 )
 *   nsec = 10^9 fmod( hi . 1.024^2, 1.0) + lo / 1.024
 */
void
amoeba_gettime(pan_time_p now)
{
    double dsecs;

    copy_timer(mapped_timer, (union sun4m_timer *)now);

    now->t_sec <<= 2;
    now->t_sec |= ((long unsigned int)(now->t_nsec) & bit_30_31) >> 30;
    (long unsigned int)(now->t_nsec) &= ~bit_30_31;

    dsecs  = now->t_sec * (1.024 * 1.024);
    now->t_sec = (long int)dsecs;
    now->t_nsec = (long int)(NANO * (dsecs - now->t_sec)) +
			(long int)(now->t_nsec * 0.9765625);
}


void
pan_sys_time_start(void)
{
    struct sun4m_timer_regs *mapped_timer_struct;

    sun4m_timer_init(NULL);
    mapped_timer_struct = sun4m_timer_getptr();
    mapped_timer = &mapped_timer_struct->pc_timer;

    pan_time_infinity = pan_time_create();
    pan_time_set(pan_time_infinity,
		 (long int)((long unsigned)(2 << 31) - 1), NANO);
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

    time = (pan_time_p)pan_malloc(sizeof(pan_time_t));

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
    *to = *from;
}


void
pan_time_get(pan_time_p now)
{
    amoeba_gettime(now);
}

void
pan_time_set(pan_time_p time, long sec, unsigned long nsec)
{
    time->t_sec  = sec;
    time->t_nsec = nsec;
}

int
pan_time_cmp(pan_time_p t1, pan_time_p t2)
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


void
pan_time_add(pan_time_p res, pan_time_p delta)
{
    res->t_nsec += delta->t_nsec; /* max 2e9 = 2^31 */
    res->t_sec  += delta->t_sec;
    if (res->t_nsec >= NANO){
	res->t_nsec -= NANO;
	res->t_sec ++;
    }
}



void
pan_time_sub(pan_time_p res, pan_time_p delta)
{
    res->t_nsec -= delta->t_nsec; /* max 2e9 = 2^31 */
    res->t_sec  -= delta->t_sec;
    if (res->t_nsec < 0) {
	res->t_nsec += NANO;
	--res->t_sec;
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
    if (pan_time_cmp(res, pan_time_zero) < 0) {	/* % operator ill-defined for negs */
	pan_time_set(&t1, 0, 0);
	pan_time_sub(&t1, res);
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
	pan_time_set(&t1, 0, 0);
	pan_time_sub(&t1, res);
	*res = t1;
    }
}



void
pan_time_mulf(pan_time_p res, double nr)
{
    pan_time_t t_int;
    pan_time_t t_frac;
    double   f;
    double   f_sec;
    double   f_nsec;
    int      is_neg;
    struct pan_time t1;

    is_neg = (nr < 0);
    if (is_neg) {
	nr = -nr;
    }
    if (pan_time_cmp(res, pan_time_zero) < 0) {		/* modf is not OK for negs */
	pan_time_set(&t1, 0, 0);
	pan_time_sub(&t1, res);
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
    pan_time_add(res, &t_frac);

    if (is_neg) {
	pan_time_set(t1, 0, 0);
	pan_time_sub(t1, res);
	*res = t1;
    }
}


void
pan_time_d2t(pan_time_p t, double d)
{
    t->t_sec  = (long int)d;
    t->t_nsec = (long int)((d - t->t_sec) * NANO);
    if (t->t_nsec < 0) {
	t->t_sec  -= 1;
	t->t_nsec += NANO;
    }
}


double
pan_time_t2d(pan_time_p t)
{
    return t->t_nsec / ((double)NANO) + t->t_sec;
}


void
pan_time_t2fix(pan_time_p t, pan_time_fix_p f)
{
    f->t_sec = t->t_sec;
    f->t_nsec = t->t_nsec;
}


int
pan_time_sub_fix_small(pan_time_p t1, pan_time_p t2, long int *d)
{
    pan_time_t     dt;

    dt = *t1;
    pan_time_sub(&dt, t2);
    if (dt.t_sec == 0) {
	*d = dt.t_nsec;
	return 1;
    }
    return 0;
}


#endif		/* __GNUC__ */


#else		/* NO_MAPPED_TIMER */


#define MILLI	1000



static void
amoeba_gettime(pan_time_p now)
{
    now->t = sys_milli();  /* no mapped timer, use sys_milli()... */
}




void
pan_sys_time_start(void)
{
    pan_time_infinity = pan_time_create();
    pan_time_infinity->t = LONG_MAX;
    pan_time_zero = pan_time_create();
    pan_time_zero->t = 0;
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

    time = (pan_time_p)pan_malloc(sizeof(pan_time_t));

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
    *to = *from;
}


void
pan_time_get(pan_time_p now)
{
    amoeba_gettime(now);
}


int
pan_time_cmp(pan_time_p t1, pan_time_p t2)
{
    if (t1->t == t2->t) {
	return 0;
    } else if (t1->t > t2->t) {
	return 1;
    } else {
	return -1;
    }
}


void pan_time_add(pan_time_p res, pan_time_p delta)
{
    res->t += delta->t;
}



void pan_time_sub(pan_time_p res, pan_time_p delta)
{
    res->t -= delta->t;
}


void pan_time_mul(pan_time_p res, int nr)
{
    res->t *= nr;
}



void pan_time_div(pan_time_p res, int nr)
{
    res->t /= nr;
}



void pan_time_mulf(pan_time_p res, double nr)
{
    res->t = (long int)(res->t * nr);
}


void
pan_time_d2t(pan_time_p t, double d)
{
    t->t = (long int)(d * MILLI);
}


double
pan_time_t2d(pan_time_p t)
{
    return t->t / ((double)MILLI);
}


void
pan_time_t2fix(pan_time_p t, pan_time_fix_p f)
{
    f->t_sec = t / 1000;
    f->t_nsec = (t % 1000) * 1000000;
}



int
pan_time_sub_fix_small(pan_time_p t1, pan_time_p t2, long int *d)
{
    *d = *t1 - *t2;
    return 1;
}

#endif		/* NO_MAPPED_TIMER */


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
