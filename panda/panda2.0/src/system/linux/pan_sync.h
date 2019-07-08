#ifndef _SYS_LINUX_SYNC_
#define _SYS_LINUX_SYNC_

#include <pthread.h>

typedef struct pan_mutex{
    pthread_mutex_t mutex;
}pan_mutex_t;

typedef struct pan_cond{
    pthread_cond_t   cond;
    pan_mutex_p      mutex;
}pan_cond_t;

extern void pan_sys_sync_start(void);
extern void pan_sys_sync_end(void);

#endif /* _SYS_LINUX_SYNC_ */
