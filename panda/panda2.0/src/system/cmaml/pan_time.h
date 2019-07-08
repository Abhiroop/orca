#ifndef _SYS_CMAML_TIME_
#define _SYS_CMAML_TIME_

#ifdef CMOST
#include <sys/time.h>
#elif AMOEBA
struct timeval {
    long tv_sec;
    long tv_usec;
};
#endif

typedef struct pan_time {
    struct timeval time;
} pan_time_t;

extern void pan_sys_time_start(void);
extern void pan_sys_time_end(void);

#endif /* _SYS_CMAML_TIME_ */
