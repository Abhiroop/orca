#include "pan_system.h"
#include "pan_time.h"
#include "pan_error.h"
#include "pan_malloc.h"

#include <assert.h>
#include <sys/time.h>
#include <sys/root.h>
#include <sys/select.h>

#include <math.h>
#include <limits.h>

#include <sys/logerror.h>

#include "pan_parix.h"

/* Parix provides us with 2 timers: Low and High.
 * High has a high granularity, so it allows to do detailed time measurements,
 * but it wraps around frequently (every 78 minutes).
 * Therefore we use Low to correct for the overflows.
 *
 * Algorithm:
 *
 * Given two times at which we measure the clocks, with resulting differences
 * dH from TimeHigh and dL from TimeLow.
 *
 * Let H = TimeHigh/time,
 *     L = TimeLow/time,
 *     W = wraparound of TimeHigh (= ULONGMAX).
 *
 * Now:
 *
 *      dt = dH / H + n W / H,			(1)
 *
 * where the number of overflows is found from reading TimeLow:
 *
 *      n  = trunc( dL . H / ( W L ) )		(2)
 *
 * Now, an extra problem surfaces, as we show by an example:
 * If the time between the first reading of lo_time and hi_time takes longer
 * than the time between the current reading of the pair of timers, this
 * formula may return the wrong time when dL is just past an overflow value:
 * dH may still be (just) below an overflow value. In this case, we must
 * subtract one from the number of overflows following from (2).
 *
 *      --n					(2a)
 *
 * The reverse problem might also occur: the second reading of TimeLow is early,
 * so it indicates one more overflow than has happenend between the readings
 * of TimeHigh. This situation we avoid by reading the timers in reverse order:
 * at the start of the timed interval we first read TimeLow then TimeHigh,
 * at the end of the timed interval we first read TimeHigh then TimeLow.
 *
 * Rutger 3 april 1995
 */


static unsigned int      first_lo_time;		/* Offset for Low time */
unsigned int             pan_time_high_first;	/* Offset for High time */
static unsigned int      wrap_lo;		/* Wrap time in Low ticks */
static unsigned int      wrap_lo_div_16;	/* Small compared to wrap_lo */
static struct pan_time   wrap_time;		/* Wrap time in panda seconds */


pan_time_p               pan_time_infinity;
pan_time_p               pan_time_zero;


/* Negative times:
 * invariant holds that time = t_sec + NANO * t_usec., with 0 <= t_usec < NANO.
 * This differs from the conventional notation, for instance:
 * time = -0.25 is represented as { -1, 750000000 }
 *
 * The easiest way to negate a time value is by subtracting it from time_zero.
 */

#define NANO 1000000000


pan_time_p
pan_time_create(void)
{
    pan_time_p time;

    time = pan_malloc(sizeof(struct pan_time));

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


void pan_time_add(pan_time_p res, pan_time_p delta)
{
    res->t_nsec += delta->t_nsec; /* max 2e9 = 2^31 */
    res->t_sec  += delta->t_sec;
    if (res->t_nsec >= NANO){
	res->t_nsec -= NANO;
	res->t_sec ++;
    }
}

void pan_time_sub(pan_time_p res, pan_time_p delta)
{
    res->t_nsec -= delta->t_nsec; /* max 2e9 = 2^31 */
    res->t_sec  -= delta->t_sec;
    if (res->t_nsec < 0) {
	res->t_nsec += NANO;
	--res->t_sec;
    }
}

#define pan_time_set(t,s,n) \
	((t)->t_sec = (s), (t)->t_nsec = (n))

#define pan_time_cmp(t1,t2) \
	(((t1)->t_sec == (t2)->t_sec) \
	 ? ((t1)->t_nsec == (t2)->t_nsec) \
	   ? 0 \
	   : ((t1)->t_nsec > (t2)->t_nsec) \
	     ? 1 \
	     : -1 \
	 : ((t1)->t_sec > (t2)->t_sec) \
	   ? 1 \
	   : -1)

#define pan_time_add(r,d) \
	((r)->t_sec += (d)->t_sec, \
	 (((r)->t_nsec += (d)->t_nsec) >= NANO \
	   ? ((r)->t_nsec -= NANO, ++(r)->t_sec) \
	   : 0))

#define pan_time_sub(r,d) \
	((r)->t_sec -= (d)->t_sec, \
	 (((r)->t_nsec -= (d)->t_nsec) < 0 \
	   ? ((r)->t_nsec += NANO, --(r)->t_sec) \
	   : 0))


void pan_time_mul(pan_time_p res, int nr)
{
    unsigned long int u;
    struct pan_time t1;

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
	pan_time_set(&t1, 0, 0);
	pan_time_sub(&t1, res);
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
    f->t_sec = t->t_sec;
    f->t_nsec = t->t_nsec;
}


void
pan_time_fix2t(pan_time_fix_p f, pan_time_p t)
{
    t->t_sec = f->t_sec;
    t->t_nsec = f->t_nsec;
}


int
pan_time_sub_fix_small(pan_time_p t1, pan_time_p t2, long int *d)
{
    struct pan_time     dt;

    dt = *t1;
    pan_time_sub(&dt, t2);
    if (dt.t_sec == 0) {
	*d = dt.t_nsec;
	return 1;
    }
    return 0;
}


#ifndef IGNORE_OVERFLOW


#  ifdef PARIX_PowerPC

/* The Parix PowerPC manual pages tell us:

     The  former  transputer  oriented  functions   TimeNowLow(),
     TimeNowHigh(),  TimeWaitLow()  and  TimeWaitHigh() are still
     supported with this release for  reasons  of  compatibility,
     but  may  be  withdrawn in future releases. For new develop-
     ments it is recommended to use TimeNow() and TimeWait().

 *
 * Fine! We want to use the same trick as on the Transputer: use the LowTimer
 * to detect wrap-arounds in the HighTimer.
 * But...
 * When the HighTimer wraps around, the LowTimer is also reset. We suspect that
 * the LowTimer is implemented by shifting the HighTimer to the right.
 * The LowTimer is unusable for our purposes.
 * Other solution:
 * TimeWait and Select(TimeAfterOption) seem to be aware of the wraparound
 * stuff. If TimeNow is not too far from wraparound, and a TimeWait is
 * requested past wraparound, this is granted correctly! The same holds for
 * Select(TimeAfterOption).
 * So we make a separate thread that awakes every LOW_PER_HIGH high ticks, and
 * increments its counter. This counter now functions as a kind of LowTimer().
 * The thread uses Select(TimeAfterOption) because it's easy to stop the
 * thread with a LocalLink Send, at shut-down time.
 */


					/* Approximate low/high ticks */
#define SECS_PER_TICK	100.0
#define LOW_PER_HIGH	((unsigned int)(SECS_PER_TICK * CLOCK_TICK))

static Thread_t    *tick_daemon;	/* Thread for the timer ticks */
static LinkCB_t    *tick_link[2];	/* Local channel to stop the timer */
static Semaphore_t  tick_lock;		/* Monitor for the timer ticks */
static unsigned int lo_timer_ticks;	/* Timer ticks */
static unsigned int lo_per_hi;		/* exact low_per_high ticks */


void
pan_time_get(pan_time_p now)
{
    unsigned long int hi_time;
    unsigned long int lo_time;
    struct pan_time   low_time;
    int               n;

    Wait(&tick_lock);
    hi_time = TimeNow() - pan_time_high_first;
    lo_time = lo_timer_ticks;
    Signal(&tick_lock);

    n = lo_time / wrap_lo;	/* First try (2a.) */
    if (lo_time - n * wrap_lo < wrap_lo_div_16 &&
				/* lo_time is greater than but close to an
				 * overflow value */
	hi_time > (ULONG_MAX >> 1)) {	/* high timer has not yet overflown */
	--n;
    }

    now->t_sec = (long)(hi_time / CLOCK_TICK);
    now->t_nsec = (long)((hi_time - now->t_sec * CLOCK_TICK) *
			 (NANO / CLOCK_TICK));

    if (n != 0) {
				/* Add time corresponding to n wraparounds */
	low_time = wrap_time;
	pan_time_mul(&low_time, n);
	pan_time_add(now, &low_time);
    }
}


static int
tick_thread(void *arg)
{
    unsigned int t;
    unsigned int delta;
    Option_t     timeout;
    Option_t     stopper;
    int          stop_val;

    t = TimeNow();
    delta = lo_per_hi;

    while (TRUE) {
	t += delta;
	timeout = TimeAfterOption(t);
	stopper = ReceiveOption(tick_link[0]);
	if (Select(2, stopper, timeout) == 0) {
					/* Someone has sent something on our
					 * channel; this cannot be but the
					 * stop message */
	    break;
	}
	Wait(&tick_lock);
	++lo_timer_ticks;
	Signal(&tick_lock);
    }

    if (RecvLink(tick_link[0], &stop_val, sizeof(int)) != sizeof(int)) {
	printe("RecvLink error on tick link in tick daemon\n");
	abort();
    }

    return 0;
}


void
pan_sys_time_start(void)
{
    double ulong_overflow;
    int    error;

    if (LocalLink(tick_link) != 0) {
	printe("Error in LocalLink for tick daemon\n");
	abort();
    }

    ulong_overflow = ((double)ULONG_MAX) + 1.0;
    pan_time_d2t(&wrap_time, ulong_overflow / CLOCK_TICK);

			/* find exact value for low_per_high from approximate
			 * LOW_PER_HIGH: low_per_high must divide
			 * ulong_overflow, therefore be a power of 2  */
    lo_per_hi = 1;
    while (lo_per_hi <= LOW_PER_HIGH) {
	lo_per_hi <<= 1;
    }
    if (lo_per_hi > 1) {
	lo_per_hi >>= 1;
    }
    wrap_lo        = (unsigned int)((ulong_overflow / lo_per_hi) + 0.5);
    wrap_lo_div_16 = wrap_lo >> 4;
    if (wrap_lo_div_16 < 1) {
	printe("Timer: granularity for tick daemon too coarse\n");
	abort();
    }

    lo_timer_ticks = 0;
    InitSem(&tick_lock, 1);
    tick_daemon = CreateThread(NULL, 0, tick_thread, &error, NULL);
    if (tick_daemon == NULL) {
	printe("Cannot start tick daemon: error %d\n", error);
	abort();
    }

    pan_time_high_first = TimeNow();
    first_lo_time = lo_timer_ticks;

    pan_time_infinity = pan_time_create();
    pan_time_set(pan_time_infinity, LONG_MAX, NANO);
    pan_time_zero = pan_time_create();
    pan_time_set(pan_time_zero, 0, 0);
}

 
void
pan_sys_time_end(void)
{
    int stop_val;

    if (SendLink(tick_link[1], &stop_val, sizeof(int)) != sizeof(int)) {
	printe("Error in SendLink to stop tick daemon\n");
	abort();
    }

    WaitThread(tick_daemon, &stop_val);
}


#  else		/* PARIX_PowerPC */


void
pan_time_get(pan_time_p now)
{
    unsigned long int hi_time;
    unsigned long int lo_time;
    struct pan_time   low_time;
    int               n;

    hi_time = TimeNowHigh() - pan_time_high_first;
    lo_time = TimeNowLow() - first_lo_time;

    n = lo_time / wrap_lo;	/* First try (2a.) */
    if (lo_time - n * wrap_lo < wrap_lo_div_16 &&
				/* lo_time is greater than but close to an
				 * overflow value */
	hi_time > (ULONG_MAX >> 1)) {	/* high timer has not yet overflown */
	--n;
    }

    now->t_sec = (long)(hi_time / CLK_TCK_HIGH);
    now->t_nsec = (long)((hi_time - now->t_sec * CLK_TCK_HIGH) *
			 (NANO / CLK_TCK_HIGH));

    if (n != 0) {
				/* Add time corresponding to n wraparounds */
	low_time = wrap_time;
	pan_time_mul(&low_time, n);
	pan_time_add(now, &low_time);
    }
}


void
pan_sys_time_start(void)
{
    double ulong_overflow;

    ulong_overflow = ((double)ULONG_MAX) + 1.0;
    pan_time_d2t(&wrap_time, ulong_overflow / CLK_TCK_HIGH);
    wrap_lo   = (unsigned int)((ulong_overflow * CLK_TCK_LOW) / CLK_TCK_HIGH);
    wrap_lo_div_16 = wrap_lo >> 4;
    if (wrap_lo_div_16 < 1) {
	printe("Timer: granularity for tick daemon too coarse\n");
	abort();
    }

    pan_time_high_first = TimeNowHigh();
    first_lo_time = TimeNowLow();

    pan_time_infinity = pan_time_create();
    pan_time_set(pan_time_infinity, LONG_MAX, NANO);
    pan_time_zero = pan_time_create();
    pan_time_set(pan_time_zero, 0, 0);
}
 
void
pan_sys_time_end(void)
{
}


#  endif		/* PARIX_PowerPC */


#else		/* IGNORE_OVERFLOW */

void
pan_time_get(pan_time_p now)
{
    unsigned long int hi_time;

    hi_time = TimeNowHigh() - pan_time_high_first;

    now->t_sec = (long)(hi_time / CLK_TCK_HIGH);
    now->t_nsec = (long)((hi_time - now->t_sec * CLK_TCK_HIGH) *
			 (NANO / CLK_TCK_HIGH));
}

void
pan_sys_time_start(void)
{
    pan_time_high_first = TimeNowHigh();

    pan_time_infinity = pan_time_create();
    pan_time_set(pan_time_infinity, LONG_MAX, NANO);
    pan_time_zero = pan_time_create();
    pan_time_set(pan_time_zero, 0, 0);
}
 
void
pan_sys_time_end(void)
{
}

#endif		/* IGNORE_OVERFLOW */
