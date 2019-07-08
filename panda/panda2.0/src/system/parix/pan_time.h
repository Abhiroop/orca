#ifndef _SYS_SOLARIS_TIME_
#define _SYS_SOLARIS_TIME_

#include <sys/time.h>

struct pan_time {
    long int    t_sec;		/* seconds */
    long int    t_nsec;		/* nano seconds */
};


extern unsigned int pan_time_high_first;


void pan_sys_time_start(void);

void pan_sys_time_end(void);


#define TRANSTIME(x) \
	((x)->t_sec * CLK_TCK_HIGH + (x)->t_nsec / 1000 + pan_time_high_first)

#endif
