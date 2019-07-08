#ifndef _SYS_T800_SYNC_
#define _SYS_T800_SYNC_

#include "pan_sys.h"

#include "pan_threads.h"

#include <sys/sem.h>

struct pan_mutex {
    Semaphore_t sema;
};

struct pan_cond {
    pan_thread_p  thread_list_head;
    pan_thread_p  thread_list_tail;
    int           count;
    Semaphore_t  *monitor;
};


extern void pan_sys_sync_start(void);

extern void pan_sys_sync_end(void);


#define pan_mutex_lock(lock)	Wait(&lock->sema)
#define pan_mutex_unlock(lock)	Signal(&lock->sema)
#define pan_mutex_trylock(lock)	TestWait(&lock->sema)

#endif
