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

#  endif		/* GNUC && ! NO_LONGLONG */

#else		/* NO_MAPPED_TIMER */

struct pan_time {
    long int          t;
};

#endif		/* NO_MAPPED_TIMER */

extern void pan_sys_time_start(void);
extern void pan_sys_time_end(void);
extern void amoeba_gettime(struct pan_time *);

#endif

