#ifndef _SYS_AMOEBA_TIME_
#define _SYS_AMOEBA_TIME_

typedef struct pan_time pan_time_t;

#ifndef NO_MAPPED_TIMER

#  if ! (defined __GNUC__) || (defined NO_LONGLONG)

struct pan_time {
    long int          t_sec;
    long int          t_nsec;
};

#  else		/* GNUC && ! NO_LONGLONG */

typedef long long int long_long;

struct pan_time {
    long_long         t;
};

/* int pan_time_cmp(pan_time_p t1, pan_time_p t2) */
#define pan_time_cmp(t1, t2) \
    (((t1)->t == (t2)->t) ? 0 : ( ((t1)->t < (t2)->t) ? -1 : 1) )


/* void pan_time_add(pan_time_p res, pan_time_p delta) */
#define pan_time_add(res, delta) \
    do { (res)->t += (delta)->t; } while (0)



/* void pan_time_sub(pan_time_p res, pan_time_p delta) */
#define pan_time_sub(res, delta) \
    do { (res)->t -= (delta)->t; } while (0)


/* void pan_time_mul(pan_time_p res, int nr) */
#define pan_time_mul(res, nr) \
    do { (res)->t *= nr; } while (0)



/* void pan_time_div(pan_time_p res, int nr) */
#define pan_time_div(res, nr) \
    do { (res)->t /= nr; } while (0)


#  endif		/* GNUC && ! NO_LONGLONG */

#else		/* NO_MAPPED_TIMER */

struct pan_time {
    long int          t;
};

#endif		/* NO_MAPPED_TIMER */

extern void pan_sys_time_start(void);
extern void pan_sys_time_end(void);

#endif

