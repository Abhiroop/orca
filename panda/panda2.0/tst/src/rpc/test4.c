#include "pan_sys.h"
#include "pan_mp.h"
#include "pan_rpc.h"
#include "pan_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
   Test rpc implementation using large request messages and multiple
   threads per platform.
*/

/* static variables */
static int            nrmsg = 100;
static int            nr_threads = 2;
static int            msg_size = 20000;
static pan_mutex_p    sync_lock;
static pan_cond_p     sync_cond;
static int            senders_ready = 0;
static pan_time_p     stime;

/*
 * sender:
 *                 Send nr_msg messages of size msg_size
 */

static void
sender(void *arg)
{
    pan_msg_p message;
    pan_msg_p reply;
    int i;

    printf("sender\n");

    message = pan_msg_create();
    (void)pan_msg_push(message, msg_size, 1);

    /* warm up communication channel */
    pan_rpc_trans(0, message, &reply);

    /* wait for signal to start */
    pan_mutex_lock(sync_lock);
    senders_ready++;
    if (senders_ready == nr_threads){
	pan_cond_broadcast(sync_cond);
    }
    while(senders_ready > 0){
	pan_cond_wait(sync_cond);
    }
    pan_mutex_unlock(sync_lock);

    for(i = 0; i < nrmsg; i++){
	pan_rpc_trans(0, message, &reply);
	pan_msg_clear(reply);
    }

    pan_msg_clear(message);

    pan_mutex_lock(sync_lock);
    senders_ready++;
    if (senders_ready == nr_threads){
	pan_cond_broadcast(sync_cond);
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
    static int total;

    pan_msg_empty(message);
    pan_rpc_reply(upcall, message);

    if (count == 0) {		/* only compute folowing expression once */
        total = (nrmsg+1) * (pan_nr_platforms() - 1) * nr_threads;
    }

    if (++count == total) {
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

    if (argc > 4){
	fprintf(stderr, "Usage: %s [nr_threads] [nr_msg] [msg_size]\n", 
		argv[0]);
	exit(1);
    }

    if (argc >= 2) nr_threads = atoi(argv[1]);
    if (argc >= 3) nrmsg = atoi(argv[2]);
    if (argc >= 4) msg_size = atoi(argv[3]);

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
	pan_mutex_lock(sync_lock);
	for(i = 0; i < nr_threads; i++){
	    /* Automatic garbage collection ;-) */
	    (void)pan_thread_create(sender, NULL, 0L, 0, 1);
	}

	/* everybody warmed up? */
	while(senders_ready < nr_threads){
	    pan_cond_wait(sync_cond);
	}

	pan_time_get(start);
	senders_ready = 0;
	pan_cond_broadcast(sync_cond);
	
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
