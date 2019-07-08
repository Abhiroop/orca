/* Straightforward priority queue implementation based on a sorted list.
 */

#ifndef PAN_PRIOQ_H
#define PAN_PRIOQ_H

typedef struct pan_prioq_imp pan_prioq_imp_t;
typedef struct pan_prioq_link pan_prioq_link_t;

#include "ot_thread.h"

/*-------------------------------------------------------------------
 * nonpreemptive FIFO Queues
 *-------------------------------------------------------------------*/

struct pan_prioq_imp {
    ot_thread_t *head;
};

struct pan_prioq_link {
    ot_thread_t *next;
};

#define PAN_PRIOQ_LINKSIZE 	sizeof (pan_prioq_link_t)
#define PAN_PRIOQ_IMPSIZE	sizeof (pan_prioq_imp_t)

extern void pan_prioq_init (pan_prioq_imp_t *q);
extern ot_thread_t *pan_prioq_get (pan_prioq_imp_t *q);
extern void pan_prioq_put (pan_prioq_imp_t *q, ot_thread_t *t);

#endif
