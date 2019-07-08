#include "pan_sys.h"
#include "pan_mp.h"
#include "pan_rpc.h"
#include "pan_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
   Test rpc implementation using small messages and multiple threads per
   platform.
*/

/* static variables */
static int            nrmsg = 1000;
static int            nr_threads = 2;
static pan_mutex_p    sync_lock;
static pan_cond_p     sync_cond;
static int            senders_ready = 0;
static pan_time_p     stime;

/*
 * sender:
 *                 Send nr_msg empty messages
 */

static void
sender(void *arg)
{
    pan_msg_p message;
    pan_msg_p reply;
    int *p, *q;
    int i;

    printf("sender\n");

    pan_sleep(stime);

    message = pan_msg_create();
    p = (int *)pan_msg_push(message, sizeof(int), alignof(int));

    for(i = 0; i < nrmsg; i++){
	*p = i;
	pan_rpc_trans(0, message, &reply);
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
	pan_cond_signal(sync_cond);
    }
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
    static int count = 0;
    pan_msg_p reply;
    int *p, *q;

/* XXX optimize: send request back iso new message */
    p = (int *)pan_msg_pop(message, sizeof(int), alignof(int));

    reply = pan_msg_create();

    q = (int *)pan_msg_push(reply, sizeof(int), alignof(int));
    *q = *p;

    pan_msg_clear(message);

    pan_rpc_reply(upcall, reply);

    if (++count == nrmsg * (pan_nr_platforms() - 1) * nr_threads){
	pan_mutex_lock(sync_lock);
	pan_cond_signal(sync_cond);
	pan_mutex_unlock(sync_lock);
    }
}




/*
 * receiver:
 *                 Initializes the RPC layer. The upcall handler
 *                 (do_receive) is registered.
 */

static void
receiver(void)
{
    printf("receiver\n");

    pan_rpc_register(do_receive);

    pan_mutex_lock(sync_lock);
    pan_cond_wait(sync_cond);

    pan_mutex_unlock(sync_lock);

    printf("Receiver finished\n");
}


/*
 * main:
 *                 Startup and shutdown
 */

int
main(int argc, char *argv[])
{
    pan_time_p start, stop;
    int i;

    pan_init(&argc, argv);

    if (argc > 3){
	fprintf(stderr, "Usage: %s [nr_threads] [nr_msg]\n", argv[0]);
	exit(1);
    }

    if (argc >= 2) nr_threads = atoi(argv[1]);
    if (argc >= 3) nrmsg = atoi(argv[2]);
    
    sync_lock = pan_mutex_create();
    sync_cond = pan_cond_create(sync_lock);

    pan_mp_init();
    pan_rpc_init();
    pan_util_init();

    stime = pan_time_create();
    pan_time_set(stime, 5L, 0UL);

    pan_start();

    start   = pan_time_create();
    stop    = pan_time_create();

    if (pan_my_pid() != 0){
	pan_time_get(start);
	
	pan_mutex_lock(sync_lock);
	for(i = 0; i < nr_threads; i++){
	    /* Automatic garbage collection :-) */
	    (void)pan_thread_create(sender, NULL, 0L, 0, 1);
	}

	while(senders_ready < nr_threads){
	    pan_cond_wait(sync_cond);
	}

	pan_time_get(stop);
	pan_time_sub(stop, start);

	printf("Sender finished: %g\n", pan_time_t2d(stop));
	
	pan_mutex_unlock(sync_lock);
    }else{
	receiver();
    }

    pan_sleep(stime);

    pan_time_clear(stime);

    pan_cond_clear(sync_cond);
    pan_mutex_clear(sync_lock);

    pan_util_end();
    pan_rpc_end();
    pan_mp_end();

    pan_end();

    return 0;
}
