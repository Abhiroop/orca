/*
 * Author: Raoul Bhoedjang, March 1994
 *
 * File continuation.c: mechanism to store and resume continuations
 *
 */

#include <stdio.h>
#include <string.h>
#include "panda/panda.h"
#include "continuation.h"
#include "rts_trace.h"


#define EXTEND_FREE_LIST   4            /* extend free lists by this much */

cont_queue_t cont_immediate_queue;
mutex_t cont_immediate_lock;

static int inline_immediates;		/* Set if no worker thread is needed */
static int cont_immediate_done = 0;
static cond_t cont_immediate_work;
static thread_t cont_immediate_thread;

/*
 * cont_immediate_worker:
 *
 * 	repeatedly fetches continuations from the immediate queue and
 * 	executes them.
 */
static void *
cont_immediate_worker(void *arg)
{
    trc_new_thread(0, "cont_immediate_worker");
    trc_event(proc_create, "cont_immediate_worker");

    sys_mutex_lock( &cont_immediate_lock);
    while (!cont_immediate_done) {
	if (cont_pending(&cont_immediate_queue)) {
	    cont_resume( &cont_immediate_queue);
	    continue; /* check cont_immediate_done too */
	}
	sys_cond_wait( &cont_immediate_work, &cont_immediate_lock);
    }
    sys_mutex_unlock( &cont_immediate_lock);

    trc_event(proc_exit, "cont_immediate_worker");
    return 0;
}


static void
enqueue(cont_t **tailp, cont_t *cont)
{
    if (!(*tailp)) {
	*tailp = cont->c_next = cont;
    } else {
	cont->c_next = (*tailp)->c_next;
	(*tailp)->c_next = cont;
	*tailp = cont;
    }
}


/* dequeue: dequeue first cont from list identified by tailp.
 */
static cont_t *
dequeue(cont_t **tailp)
{
    cont_p ret;

    if (!(*tailp)) {
	return (cont_t *)0;
    }

    ret = (*tailp)->c_next;
    if (ret == ret->c_next) {
	*tailp = (cont_t *)0;
    } else {
	(*tailp)->c_next = ret->c_next;
    }
    ret->c_next = (cont_t *)0;

    return ret;   /* return the head */
}


static void
concat(cont_t **q1, cont_t *q2)
{
    cont_t *head1;

    if (!(*q1)) {              /* q1 empty, easy case */
	*q1 = q2;
    } else if (q2) {           /* both nonempty: concatenate */
	head1 = (*q1)->c_next;
	(*q1)->c_next = q2->c_next;
	(q2)->c_next = head1;
	*q1 = q2;
    }
}


/* If immediate continuations may not be inlined, then start a worker
 * thread that listens to the special "immediate queue" and
 * executes any continuations on that queue.
 */
void 
cont_start(int inline_immediate_conts)
{
    sys_mutex_init( &cont_immediate_lock);
    cont_init(&cont_immediate_queue, &cont_immediate_lock);

    inline_immediates = inline_immediate_conts;
    if (!inline_immediate_conts) {
	sys_cond_init( &cont_immediate_work);
	sys_thread_create( &cont_immediate_thread, cont_immediate_worker,
		      (void *)0, 0, 0);
    }
}

void
cont_end(void)
{
    if (!inline_immediates) {
	sys_mutex_lock( &cont_immediate_lock);
	cont_immediate_done = 1;
	sys_cond_broadcast( &cont_immediate_work);
	sys_mutex_unlock( &cont_immediate_lock);
	sys_thread_join( &cont_immediate_thread);
	sys_cond_clear( &cont_immediate_work);
    }
    
    cont_clear(&cont_immediate_queue);
    sys_mutex_clear( &cont_immediate_lock);
}

/*
 * Initialise a continuation queue. Both the queue and its free list are
 * initially empty.
 */
void
cont_init(cont_queue_t *q, mutex_t *lock)
{
    q->cq_lock      = lock;
    q->cq_freelist  = (cont_t *)0;
    q->cq_contlist  = (cont_t *)0;
    q->cq_allocated = 0;
}


/*
 * All allocated continuations should be on the free list. Complain if
 * some are missing.
 */
void
cont_clear(cont_queue_t *q)
{
    cont_t *cont;

    sys_mutex_lock(q->cq_lock);

    if (q->cq_allocated != 0) {
	printf("(warning) pending continuations (q->cq_allocated = %d)\n",
	       q->cq_allocated);
    }
    while ((cont = dequeue(&q->cq_freelist))) {
	sys_cond_clear(&cont->c_cond);
	sys_free(cont);
    }

    sys_mutex_unlock(q->cq_lock);
}


/* cont_alloc: allocate a cont from the free list; if it is
 * empty then extend it.
 */
void *
cont_alloc(cont_queue_t *q, int size, int (*func)(void *state, mutex_t *lock))
{
    unsigned i;
    cont_t *cont;

    assert(size <= CONT_SIZE);
    assert(!sys_mutex_trylock(q->cq_lock));     /* lock must be held! */

    if (!q->cq_freelist) {
	for (i = 0; i < EXTEND_FREE_LIST; i++) {
	    cont = (cont_t *)sys_calloc(1, sizeof(*cont));
	    sys_cond_init(&cont->c_cond);
	    enqueue(&q->cq_freelist, cont);
	}
    }

    cont = dequeue(&q->cq_freelist);

    q->cq_allocated++;

    cont->c_queue = q;             /* back pointer to continuation queue */
    cont->c_func  = func;          /* the continuation function */
    cont->c_next  = (cont_t *)0;   /* next in queue */

    return (void *)(&cont->c_buf);
}


/*
 * cont_save:
 *
 * 	The behaviour of cont_save is controlled by three parameters:
 * 	1. value inline_immediates (true/false)
 * 	2. value of block_me       (true/false)
 * 	3. value of cont. queue    (cont_immediate_queue or other
 * 				    queue)
 *
 */
void
cont_save(void *buf, int block_me)
{
    cont_t *cont;
    cont_queue_t *q;
    int status;
    
    cont = (cont_t *)buf;
    cont->c_thread = block_me;
    q = cont->c_queue;
    
    assert(!sys_mutex_trylock(q->cq_lock));
    
    if (q == &cont_immediate_queue) {
	assert(!block_me);
	if (inline_immediates) {
	    status = (*cont->c_func)(&cont->c_buf, q->cq_lock);
	    assert(status == CONT_NEXT);
	    enqueue(&q->cq_freelist, cont);
	    q->cq_allocated--;
	    return;
	} else {
	    enqueue(&q->cq_contlist, cont);
	    sys_cond_broadcast(&cont_immediate_work);
	}
    } else {
	enqueue(&q->cq_contlist, cont);
	if (!block_me) {
	    return;
	}
	do {
	    sys_cond_wait(&cont->c_cond, q->cq_lock);
	
	    /* Invoke the continuation.
	     * If it completes, then free the cont node, otherwise
	     * requeue it. 
	     */
	    status = (*cont->c_func)(&cont->c_buf, q->cq_lock);
		
	    switch (status) {
	      case CONT_KEEP:
		enqueue(&q->cq_contlist, cont);
		break;

	      case CONT_NEXT:
		enqueue(&q->cq_freelist, cont);
		q->cq_allocated--;
		break;
	
	      case CONT_RESTART:
		enqueue(&q->cq_freelist, cont);
		q->cq_allocated--;
		cont_resume(q);
		break;
	
	      default:
		assert(0);
	    }
	} while (status == CONT_KEEP);
    }
}


/* cont_resume: try to execute continuations on list q.
 * warning: make sure that "new" continuations are queued at the end.
 */
void cont_resume(cont_queue_t *q)
{
    cont_t *tmp_list, *cont;
    int status;
    
    assert(!sys_mutex_trylock(q->cq_lock));
    
    tmp_list = 0;
    while ((cont = dequeue(&q->cq_contlist))) {
	/* Invoke the continuation.
	 * If it completes, then free the cont node, otherwise
	 * requeue it. 
	 */
	
	if (cont->c_thread) {
	    sys_cond_signal(&cont->c_cond);
	} else {
	    assert(cont->c_func);
	    status = (*cont->c_func)(&cont->c_buf, q->cq_lock);
	    
	    switch (status) {
	      case CONT_KEEP:
		enqueue(&tmp_list, cont);
		break;

	      case CONT_NEXT:
		enqueue(&q->cq_freelist, cont);
		q->cq_allocated--;
		break;

	      case CONT_RESTART:
		enqueue(&q->cq_freelist, cont);
		q->cq_allocated--;
		concat(&tmp_list, q->cq_contlist);
		q->cq_contlist = tmp_list;
		tmp_list = 0;
		break;

	      default:
		assert(0);
	    }
	}
    }
    q->cq_contlist = tmp_list;
}
