#ifndef __RTS_TPOOL_H__
#define __RTS_TPOOL_H__

#include "communication.h"

typedef struct rts_tpool *rts_tpool_p;

void rts_tpool_init(void);
void rts_tpool_end(void);

#define DYNAMIC_POOL 0

rts_tpool_p rts_tpool_create(int nr_threads);
void        rts_tpool_destroy(rts_tpool_p pool);

void rts_tpool_add(rts_tpool_p pool, message_p m);



#endif /* __RTS_TPOOL_H__ */
