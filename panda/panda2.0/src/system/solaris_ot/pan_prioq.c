#include "pan_prioq.h"
#include "pan_sys.h"
#include "pan_threads.h"

/*-------------------------------------------------------------------
 * pan_prioq_init
 *-------------------------------------------------------------------*/

void pan_prioq_init (pan_prioq_imp_t *q)
{
    q->head = NULL;
}

/*-------------------------------------------------------------------
 * pan_prioq_get
 *-------------------------------------------------------------------*/

ot_thread_t *pan_prioq_get (pan_prioq_imp_t *q)
{
    ot_thread_t *t;

    t = q->head;
    if (t != NULL) {
	q->head = ((pan_prioq_link_t *)t->qlink)->next;
    }
    return t;
}

/*-------------------------------------------------------------------
 * pan_prioq_put
 *-------------------------------------------------------------------*/

static
int get_prio( ot_thread_t *t)
{
    pan_thread_p thr = ot_thread_getspecific( t);

    /* Check for non-panda threads; run them at highest priority */
    return (thr == NULL ? 100000 : thr->prio);
}

void pan_prioq_put (pan_prioq_imp_t *q, ot_thread_t *t)
{
    ot_thread_t *prev, *cur;
    int prio = get_prio( t);

    if (q->head == NULL || get_prio( q->head) < prio) {
	((pan_prioq_link_t *)t->qlink)->next = q->head;
	q->head = t;
    }
    else {
	for ( prev = q->head, cur = ((pan_prioq_link_t *)prev->qlink)->next;
	      cur != NULL &&  get_prio( cur) >= prio;
	      prev = cur, cur = ((pan_prioq_link_t *)cur->qlink)->next) {
	}
	((pan_prioq_link_t *)t->qlink)->next = cur;
	((pan_prioq_link_t *)prev->qlink)->next = t;
    }
}
