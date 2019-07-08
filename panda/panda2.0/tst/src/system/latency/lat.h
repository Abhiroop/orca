#ifndef _PANDA_SMALL_
#define _PANDA_SMALL_

#include "pan_sys.h"

typedef struct{
    int seqno;			/* Used inside protocol layer */
    int data;			/* User data */
}header_t, *header_p;

extern void pan_small_send(header_p header);
extern void pan_small_register(void (*func)(header_p header));
extern void pan_small_start(void);
extern void pan_small_end(void);

#endif /* _PANDA_SMALL_ */
