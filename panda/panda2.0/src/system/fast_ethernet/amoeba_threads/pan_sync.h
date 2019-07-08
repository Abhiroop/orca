#ifndef _SYS_AMOEBA_SYNC_
#define _SYS_AMOEBA_SYNC_

#include <amoeba.h>
#include "module/mutex.h"	/* Use Koen's superfast mutex */

struct pan_mutex{
    mutex lock;
};

struct waiter{
    mutex          lock;
    int            prio;
    struct waiter *next;
};

struct pan_cond{
    struct waiter  sentinel;
    pan_mutex_p    lock;
};

extern void pan_sys_sync_start(void);
extern void pan_sys_sync_end(void);

void pan_mutex_init(pan_mutex_p lock);

/* Koen: inline mutex calls inside the system layer */

#define pan_mutex_lock(l)       mu_lock(&(l)->lock)
#define pan_mutex_unlock(l)     mu_unlock(&(l)->lock)
#define pan_mutex_trylock(l)    (!mu_trylock(&(l)->lock, (interval)0))

#endif
