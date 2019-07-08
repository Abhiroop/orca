#ifndef __RTS_ATOMIC_INT_H__
#define __RTS_ATOMIC_INT_H__

#include "pan_sys.h"

struct atomic_int_s {
    int         value;		/* Value of the atomic integer. */
    pan_mutex_p lock;		/* Lock used to implement atomicity. */
};


#endif /* __RTS_ATOMIC_INT_H__ */
