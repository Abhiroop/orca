#ifndef _SYS_LINUX_TIME_
#define _SYS_LINUX_TIME_

#include <pthread.h>
#include <signal.h>

struct pan_time {
    struct timespec time;
};

typedef struct pan_time pan_time_t;

extern void pan_sys_time_start(void);
extern void pan_sys_time_end(void);

#endif

