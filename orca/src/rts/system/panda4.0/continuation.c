/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/*
 * Author: Raoul Bhoedjang, March 1994
 *
 * File continuation.c: mechanism to store and resume continuations
 *
 * 14/11/95 Koen: a blocking thread no longer evaluates the function itself.
 *
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pan_sys.h"
#include "continuation.h"
#include "rts_globals.h"
#include "rts_internals.h"
#include "rts_trace.h"


#define EXTEND_FREE_LIST   4		/* extend free lists by this much */

cont_queue_t cont_immediate_queue;

static int inline_immediates;		/* Set if no worker thread is needed */
static int cont_immediate_done = 0;
static pan_cond_p cont_immediate_work;
static pan_thread_p cont_immediate_thread;

/*
 * cont_immediate_worker:
 *
 * 	repeatedly fetches continuations from the immediate queue and
 * 	executes them.
 */
static void
cont_immediate_worker(void *arg)
{
    trc_event(proc_create, "cont_immediate_worker");

    rts_lock();
    while (!cont_immediate_done) {
	if (cont_pending(&cont_immediate_queue)) {
	    cont_resume( &cont_immediate_queue);
	    continue; /* check cont_immediate_done too */
	}
	pan_cond_wait(cont_immediate_work);
    }
    rts_unlock();

    trc_event(proc_exit, "cont_immediate_worker");
    pan_thread_exit();
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

    if (!(*q1)) {		/* q1 empty, easy case */
	*q1 = q2;
    } else if (q2) {		/* both nonempty: concatenate */
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
    cont_init(&cont_immediate_queue);

    inline_immediates = inline_immediate_conts;
    if (!inline_immediate_conts) {
	cont_immediate_work = rts_cond_create();
	cont_immediate_thread = pan_thread_create(cont_immediate_worker,
						  (void *)0, 0, 0, 0,
						  "cont immediate worker");
    }
}

void
cont_end(void)
{
    if (!inline_immediates) {
	cont_immediate_done = 1;
	pan_cond_broadcast(cont_immediate_work);
	rts_unlock();
	pan_thread_join(cont_immediate_thread);
	rts_lock();
	rts_cond_clear(cont_immediate_work);
    }
    
    cont_clear(&cont_immediate_queue);
}

/*
 * Initialise a continuation queue. Both the queue and its free list are
 * initially empty.
 */
void
cont_init(cont_queue_t *q)
{
    q->cq_freelist  = 0;
    q->cq_contlist  = 0;
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

    /* rts_lock() */

    if (q->cq_allocated != 0) {
	printf("(warning) pending continuations (q->cq_allocated = %d)\n",
	       q->cq_allocated);
    }
    while ((cont = dequeue(&q->cq_freelist))) {
	rts_cond_clear(cont->c_cond);
	m_free(cont);
    }

    /* rts_unlock() */
}


/* cont_alloc: allocate a cont from the free list; if it is
 * empty then extend it.
 */
void *
cont_alloc(cont_queue_t *q, int size, int (*func)(void *state))
{
    unsigned i;
    cont_t *cont;

    assert(size <= CONT_SIZE);
    assert(!rts_trylock());     /* lock must be held! */

    if (!q->cq_freelist) {
	for (i = 0; i < EXTEND_FREE_LIST; i++) {
	    cont = (cont_t *)m_malloc(sizeof(*cont));
	    (void)memset(cont, 0, sizeof(*cont));
	    cont->c_cond = rts_cond_create();
	    enqueue(&q->cq_freelist, cont);
	}
    }

    cont = dequeue(&q->cq_freelist);

    q->cq_allocated++;

    cont->c_queue = q;			/* back pointer to continuation queue */
    cont->c_func  = func;		/* the continuation function */
    cont->c_next  = (cont_t *)0;	/* next in queue */

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
    
    assert(!rts_trylock());
    
    if (q == &cont_immediate_queue) {
	assert(!block_me);
	if (inline_immediates) {
	    status = (*cont->c_func)(&cont->c_buf);
	    assert(status == CONT_NEXT);
	    enqueue(&q->cq_freelist, cont);
	    q->cq_allocated--;
	} else {
	    enqueue(&q->cq_contlist, cont);
	    pan_cond_broadcast(cont_immediate_work);
	}
    } else {
	enqueue(&q->cq_contlist, cont);
	if (block_me) {
	    pan_cond_wait(cont->c_cond);
	}
    }
}


/* cont_resume: try to execute continuations on list q.
 * warning: make sure that "new" continuations are queued at the end.
 */
void
cont_resume(cont_queue_t *q)
{
    cont_t *tmp_list, *cont;
    int status;
    
    assert(!rts_trylock());
    
    tmp_list = 0;
    while ((cont = dequeue(&q->cq_contlist))) {
	/* Invoke the continuation.
	 * If it completes, then free the cont node, otherwise
	 * requeue it. 
	 */
	
	assert(cont->c_func);
	status = (*cont->c_func)(&cont->c_buf);
	    
	switch (status) {
	  case CONT_KEEP:
	    enqueue(&tmp_list, cont);
	    break;

	  case CONT_NEXT:
	    if (cont->c_thread) {
	    	pan_cond_signal(cont->c_cond);
	    }
	    enqueue(&q->cq_freelist, cont);
	    q->cq_allocated--;
	    break;

	  case CONT_RESTART:
	    if (cont->c_thread) {
	    	pan_cond_signal(cont->c_cond);
	    }
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
    q->cq_contlist = tmp_list;
}
