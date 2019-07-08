#include "pan_sys.h"
#include "pan_mp.h"
#include "pan_rpc.h"
#include "pan_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#define MAX_THREADS 100

/*
   Test rpc implementation using small messages and multiple threads per
   platform. Use a daemon process to reply messages in a random order. No
   timing, just an implementation check. Only two processors.
*/

/* static variables */
static int            nr_msg = 1000;
static int            nr_threads = 2;
static pan_mutex_p    sync_lock;
static pan_cond_p     sync_cond;
static int            senders_ready = 0;
static int            rcv_count;
static pan_time_p     stime;


typedef struct reply_queue{
    pan_upcall_p upcall;
    pan_msg_p    reply;
    int          seqno;
}reply_queue_t, *reply_queue_p;

static int            nr;
static reply_queue_t  queue[MAX_THREADS];
static pan_mutex_p    queue_lock;
static pan_cond_p     queue_cond;

/*
 * sender:
 *                 Send nr_msg empty messages
 */

static void
sender(void *arg)
{
    pan_msg_p message;
    pan_msg_p reply;
    int me = pan_my_pid();
    int *p, *q;
    int i;

    pan_sleep(stime);

    printf("In sender thread\n");

    message = pan_msg_create();
    p = (int *)pan_msg_push(message, sizeof(int), alignof(int));

    for(i = 0; i < nr_msg; i++){
	*p = i;
	printf("Sending request\n");
	pan_rpc_trans(1 - me, message, &reply);
	q = (int *)pan_msg_pop(reply, sizeof(int), alignof(int));
	if (*p != *q){
	    fprintf(stderr, "ERROR in reply message\n");
	    exit(1);
	}
	pan_msg_clear(reply);
    }

    pan_msg_clear(message);

    pan_mutex_lock(sync_lock);
    senders_ready++;
    if (senders_ready == nr_threads){
	printf("Signal from sender\n");
	pan_cond_signal(sync_cond);
    }
    pan_mutex_unlock(sync_lock);

    pan_thread_exit();
}


static void
reply_daemon(void *arg)
{
    reply_queue_t e;
    int *q, i;

    printf("In reply daemon\n");

    pan_mutex_lock(queue_lock);

    while (rcv_count < nr_msg * nr_threads){
	while(nr == 0){
	    printf("Reply daemon waiting\n");
	    pan_cond_wait(queue_cond);
	}

	i = rand() % nr;
	
	e = queue[i];
	queue[i] = queue[nr - 1];
	nr--;
	
	q = (int *)pan_msg_push(e.reply, sizeof(int), alignof(int));
	*q = e.seqno;
	
	printf("Sending reply from daemon\n");
	pan_rpc_reply(e.upcall, e.reply);

	rcv_count++;

	if (rcv_count % 100 == 0){
	    printf("%d) Replied %d requests\n", pan_my_pid(), rcv_count);
	}
    }

    pan_mutex_unlock(queue_lock);

    pan_mutex_lock(sync_lock);
    printf("Signal from reply daemon\n");
    pan_cond_signal(sync_cond);
    pan_mutex_unlock(sync_lock);

    pan_thread_exit();
}
	
	


/*
   do_receive:
                   Function called by the RPC module to handle a request
                   message. It sends a reply back. The reply messsage is
                   cleared by the RPC layer.
*/

static void
do_receive(pan_upcall_p upcall, pan_msg_p message)
{
    int p;

    printf("In do_receive\n");

    pan_mutex_lock(queue_lock);

    queue[nr].upcall = upcall;
    queue[nr].seqno = p = *(int *)pan_msg_pop(message, sizeof(int), alignof(int));
    queue[nr].reply = pan_msg_create();

    pan_msg_clear(message);
    

    if (nr++ == 0){
	pan_cond_signal(queue_cond);
    }    
    assert(nr <= MAX_THREADS);

    pan_mutex_unlock(queue_lock);
}


/*
 * main:
 *                 Startup and shutdown
 */

int
main(int argc, char *argv[])
{
    int i;

    pan_init(&argc, argv);

    if (argc > 3){
	fprintf(stderr, "Usage: %s [nr_threads] [nr_msg]\n", argv[0]);
	exit(1);
    }

    if (argc >= 2) nr_threads = atoi(argv[1]);
    if (argc >= 3) nr_msg = atoi(argv[2]);
    
    assert(pan_nr_platforms() == 2);
    assert(nr_threads <= MAX_THREADS);

    sync_lock = pan_mutex_create();
    sync_cond = pan_cond_create(sync_lock);
    queue_lock = pan_mutex_create();
    queue_cond = pan_cond_create(queue_lock);

    pan_mp_init();
    pan_rpc_init();
    pan_util_init();

    stime = pan_time_create();
    pan_time_set(stime, 5L, 0UL);

    pan_rpc_register(do_receive);

    (void)pan_thread_create(reply_daemon, NULL, 0L, 0, 1);

    pan_start();

    pan_mutex_lock(sync_lock);
    for(i = 0; i < nr_threads; i++){
	/* Automatic garbage collection :-) */
	(void)pan_thread_create(sender, NULL, 0L, 0, 1);
    }

    printf("Waiting for sending threads\n");
    while(senders_ready < nr_threads){
	pan_cond_wait(sync_cond);
    }

    printf("%d) Sender threads finished\n", pan_my_pid());

    while(rcv_count < nr_msg * nr_threads){
	pan_cond_wait(sync_cond);
    }

    printf("%d) Receiving ready\n", pan_my_pid());
    
    pan_mutex_unlock(sync_lock);

    pan_time_clear(stime);

    pan_cond_clear(sync_cond);
    pan_mutex_clear(sync_lock);
    pan_cond_clear(queue_cond);
    pan_mutex_clear(queue_lock);

    pan_util_end();
    pan_rpc_end();
    pan_mp_end();

    pan_end();

    return 0;
}
