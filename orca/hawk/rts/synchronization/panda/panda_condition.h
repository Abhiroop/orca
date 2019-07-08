#ifndef __RTS_CONDITION_H__
#define __RTS_CONDITION_H__

#include "pan_sys.h"

struct condition_s {
    pan_mutex_p lock;
    pan_cond_p  cond;
    pan_time_p  time;
    int         state;
};


#endif /* __RTS_CONDITION_H__ */
