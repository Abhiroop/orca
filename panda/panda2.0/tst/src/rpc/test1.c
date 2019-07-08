#include "pan_sys.h"
#include "pan_mp.h"
#include "pan_rpc.h"
#include "pan_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
   Test rpc implementation using empty messages
*/


typedef struct warmup{
    pan_upcall_p upcall;
    pan_msg_p    reply;
}warmup_t, warmup_p;

/* static variables */
static int            nrmsg = 1000;
static int            nrtests = 1;
static pan_mutex_p    sync_lock;
static pan_cond_p     sync_cond;
static warmup_t      *warmup;
static int            warmup_nr = 0;
static int            started = 0;
static pan_time_p     stime;

/*
 * sender:
 *                 Send nr_msg empty messages
 */

static void
sender(void)
{
    pan_time_p start, stop;
    pan_msg_p message;
    pan_msg_p reply;
    int i, j;

    printf("sender\n");

    pan_sleep(stime);

    message = pan_msg_create();
    start   = pan_time_create();
    stop    = pan_time_create();

    /* Synchronize with receiver before starting timer */
    pan_rpc_trans(0, message, &reply);
    pan_msg_clear(reply);

    for(j = 0; j < nrtests; j ++){
	pan_time_get(start);

	for(i = 0; i < nrmsg; i++){
	    pan_rpc_trans(0, message, &reply);
	    pan_msg_clear(reply);
	}

	pan_time_get(stop);
	pan_time_sub(stop, start);

	printf("Sender finished: %g\n", pan_time_t2d(stop));
    }

    pan_msg_clear(message);
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
    int i;

#ifdef DEBUG
    printf("In do_receive (started %d)\n", started);
#endif

    if (started){
	pan_rpc_reply(upcall, message);
	
	if (++count == nrtests * nrmsg * (pan_nr_platforms() - 1)){
	    pan_cond_signal(sync_cond);
	}
    }else{
	warmup[warmup_nr].upcall = upcall;
	warmup[warmup_nr].reply = message;
	
	if (++warmup_nr < pan_nr_platforms() - 1) return;

	for(i = 0; i < warmup_nr; i++){
	    pan_rpc_reply(warmup[i].upcall, warmup[i].reply);
	}

	printf("Warming up phase finished\n");
	started = 1;
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

    pan_sleep(stime);

    printf("Receiver finished\n");
}


/*
 * main:
 *                 Startup and shutdown
 */

int
main(int argc, char *argv[])
{
    pan_init(&argc, argv);

    if (argc > 3){
	fprintf(stderr, "Usage: %s [nr_msg] [nr_tests]\n", argv[0]);
	exit(1);
    }

    if (argc >= 2) nrmsg = atoi(argv[1]);
    if (argc >= 3) nrtests = atoi(argv[2]);

    sync_lock = pan_mutex_create();
    sync_cond = pan_cond_create(sync_lock);

    pan_mp_init();
    pan_rpc_init();
    pan_util_init();

    stime = pan_time_create();
    pan_time_set(stime, 5L, 0UL);

    if (pan_my_pid() == 0){
	warmup = malloc(pan_nr_platforms() * sizeof(warmup_t));
	if (!warmup){
	    fprintf(stderr, "Can't create warmup message buffer\n");
	    exit(1);
	}
    }

    pan_start();

    if (pan_my_pid() != 0){
	sender();
    }else{
	receiver();
    }

    pan_util_end();
    pan_rpc_end();
    pan_mp_end();

    pan_time_clear(stime);

    pan_cond_clear(sync_cond);
    pan_mutex_clear(sync_lock);

    pan_end();

    return 0;
}

