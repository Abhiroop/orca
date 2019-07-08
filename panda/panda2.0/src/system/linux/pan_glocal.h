#ifndef _SYS_LINUX_GLOCAL_
#define _SYS_LINUX_GLOCAL_

#include <pthread.h>

typedef struct pan_key{
    pthread_key_t key;
}pan_key_t;

extern void pan_sys_glocal_start(void);
extern void pan_sys_glocal_end(void);

#endif
