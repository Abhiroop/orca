/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"
#include "pan_mp_queue.h"
#include "pan_mp_policy.h"
#include "pan_mp_state.h"
#include "pan_mp_error.h"

#ifdef SEND_DAEMON


/*
 * Lazy implementation of a message queue. The entries that contain a
 * message which remaining fragments need to be send are placed in this
 * queue, and blasted on the network by a daemon thread.
 *
 * The size of the queue should be dynamic. For the moment, we define a
 * maximum size. All calls assume that state_lock is granted.
 */


#define MAX_QUEUE 20

#define INC(x) (x) = (((x) + 1) % MAX_QUEUE)

static void      *queue[MAX_QUEUE]; /* queue entries */
static int        head, tail;	/* head and tail of queue */
static int        size;		/* nr entries in the queue */
static pan_cond_p cond;		/* condition synchronization on overflow */




void
pan_mp_queue_start(void)
{
}


void
pan_mp_queue_end(void)
{
    pan_cond_clear(cond);
}

void
pan_mp_queue_lock(pan_mutex_p lock)
{
    cond = pan_cond_create(lock);
}


/*
 * queue_enqueue
 *                 Add an entry to the queue
 */

void
pan_mp_queue_enqueue(void *p)
{
    int i;
    
    /* wait for table entry */
    while(size == MAX_QUEUE){
	pan_mp_warning(QUEUE_FULL, 
		       "server queue full. Could cause deadlock.\n");
	pan_cond_wait(cond);
    }
    
    /* Signal possible pending dequeue */
    if (size++ == 0){
	pan_cond_broadcast(cond);
    }
    i = head;
    INC(head);

    /* Fill in state pointer */
    queue[i] = p;
}


/*
 * queue_dequeue
 *                 Get an entry from the queue. This operation blocks if
 *                 the queue is empty.
 */

void *
pan_mp_queue_dequeue(void)
{
    int i;

    /* Wait for an entry */
    while (size == 0){
	pan_cond_wait(cond);
    }

    /* Signal blocked enqueue operations */
    if (size-- == MAX_QUEUE){
	pan_cond_signal(cond);
    }
    i = tail;
    INC(tail);

    return queue[i];
}
    
#endif /* SEND_DAEMON */
