/*
 * Author:         Tim Ruhl
 *
 * Date:           Nov 30, 1995
 *
 * Thread pool.
 *        Manage a thread pool that will make upcalls with RTS
 *        messages. A message contains a handler function that should be
 *        called.
 */

#include "tpool.h"
#include "pan_sys.h"
#include "util.h"

#include <assert.h>

typedef struct node {
    message_p    msg;
    struct node *next;
}node_t, *node_p;

typedef struct rts_tpool {
    pan_mutex_p lock;
    pan_cond_p  cond;
    int         dynamic;
    int         nr_blocked;
    int         nr_active;

    int         state;
    int         nr;
    node_p      head;
    node_p      tail;
} rts_tpool_t;
    
#define STATE_ACTIVE 0
#define STATE_END    1

void 
rts_tpool_init(void)
{
}

void 
rts_tpool_end(void)
{
}

/*
 * enqueue:
 *                 Enqueue a message in order.
 */
static void
enqueue(rts_tpool_p p, message_p m)
{
    node_p n;

    n = pan_malloc(sizeof(node_t));
    
    n->msg = m;
    n->next = NULL;

    if (p->nr == 0) {
	p->head = n;
    }else {
	p->tail->next = n;
    }
    p->tail = n;

    p->nr++;
}

/*
 * dequeue:
 *                 Dequeue a message.
 */
static message_p
dequeue(rts_tpool_p p)
{
    message_p m;
    node_p n;

    assert(p->nr >= 0);
    if (p->nr == 0) return NULL;

    n = p->head;
    p->head = n->next;

    p->nr--;
    if (p->nr == 0){
	assert(p->head == NULL);
	p->tail = NULL;
    }

    m = n->msg;

    pan_free(n);

    return m;
}
    

/*
 * handler:
 *                 Waits for a message to get enqueued and makes the upcall.
 */
static void
handler(void *arg)
{
    rts_tpool_p p = (rts_tpool_p)arg;
    message_handler_p h;
#ifdef NO
    pan_thread_p thr;
#endif
    message_p m;

    pan_mutex_lock(p->lock);

    while(p->state == STATE_ACTIVE) {
	while ( (m = dequeue(p)) ) {
	    h = message_get_handler(m);
	    
	    /* Unlock pool lock and make upcall */
	    p->nr_active++;
	    pan_mutex_unlock(p->lock);
	    if ((*h)(m) == 0) {
	        free_message(m);
	    }
	    pan_mutex_lock(p->lock);
	    p->nr_active--;
	}	    

	p->nr_blocked++;
	pan_cond_wait(p->cond);
	p->nr_blocked--;
    }

    /* Wakeup caller of rts_tpool_destroy */
    pan_cond_broadcast(p->cond);

    pan_mutex_unlock(p->lock);

#ifdef NO
    /* It's not clear whether this is valid, but probably not */
    thr = pan_thread_self();
    pan_thread_clear(thr);
#endif
    pan_thread_exit();
}


rts_tpool_p 
rts_tpool_create(int nr_threads)
{
    rts_tpool_p p;
    int i;

    p = pan_malloc(sizeof(rts_tpool_t));

    p->lock = pan_mutex_create();
    p->cond = pan_cond_create(p->lock);
    p->dynamic = (nr_threads == DYNAMIC_POOL);

    p->nr_blocked = 0;
    p->nr_active = 0;
    p->state = STATE_ACTIVE;
    p->nr = 0;
    p->head = p->tail = NULL;

    /* Create static threads */
    for (i = 0; i < nr_threads; i++) {
	pan_thread_create(handler, (void *)p, 0L, 0, 1, 
			  "static saniya RTS pool thread");
    }

    return p;
}

void        
rts_tpool_destroy(rts_tpool_p p)
{
    pan_mutex_lock(p->lock);

    assert(p->state == STATE_ACTIVE);
    p->state = STATE_END;
    pan_cond_broadcast(p->cond);

    /* Wait until all messages have been handled */
    while(p->nr > 0 || p->nr_blocked > 0 || p->nr_active > 0) {
	pan_cond_wait(p->cond);
    }
    assert(p->nr_blocked == 0);

    p->nr = -1;
    p->head = p->tail = (node_p)1;

    pan_mutex_unlock(p->lock);

    pan_cond_clear(p->cond);
    pan_mutex_clear(p->lock);

    pan_free(p);
}


void 
rts_tpool_add(rts_tpool_p p, message_p m)
{
    pan_mutex_lock(p->lock);

    assert(p->state == STATE_ACTIVE);

    enqueue(p, m);

    if (p->nr_blocked == 0 && p->dynamic) {
	/* Add extra thread to dynamic pool */
	pan_thread_create(handler, (void *)p, 0L, 0, 1,
			  "dynamic saniya RTS pool thread");
    }

    pan_cond_signal(p->cond); /* Wakeup a handler thread */

    pan_mutex_unlock(p->lock);
}

