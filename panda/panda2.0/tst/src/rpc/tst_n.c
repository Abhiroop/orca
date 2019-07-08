#include "pan_sys_msg.h"
#include "pan_rpc.h"
#include "pan_util.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/*
   Test rpc implementation using n clients and n servers
*/


/* static variables */
static int            nrmsg;
static int            request_size = 0;
static int            reply_size = 0;

static pan_nsap_p     stop_nsap;
static pan_mutex_p    sync_lock;
static pan_cond_p     sync_cond;
static int            n_stops;




/*
 * client:
 *                 Send nr_msg empty messages
 */
static void
client(int server)
{
    pan_time_p start, stop;
    pan_time_p t = pan_time_create();
    pan_msg_p message;
    pan_msg_p reply;
    double throughput;
    int i;

    printf("%2d: client, send %d msgs\n", pan_my_pid(), nrmsg);

    /*
    pan_time_d2t(t, 5.0);
    pan_sleep(t);
    */

    start   = pan_time_create();
    stop    = pan_time_create();

    message = pan_msg_create();
    if (request_size > 0) {
	pan_msg_push(message, request_size, alignof(char));
    }

    pan_time_get(start);

    for (i = 0; i < nrmsg; i++){
	pan_rpc_trans(server, message, &reply);
	pan_msg_clear(reply);
    }

    pan_time_get(stop);
    pan_time_sub(stop, start);

    pan_comm_unicast_small(0, stop_nsap, NULL);

    printf("%2d: %d rpcs: %f s; = ",
	    pan_my_pid(), nrmsg, pan_time_t2d(stop));
    pan_time_div(stop, nrmsg);
    throughput = (request_size + reply_size) / (1048576 * pan_time_t2d(stop));
    printf("%f; thr = %f MB/s\n", pan_time_t2d(stop), throughput);

    pan_time_clear(t);
    pan_msg_clear(message);
}



/*
   do_receive:
                   Function called by the RPC module to handle a request
                   message. It sends a reply back. The reply messsage is
                   cleared by the RPC layer.
*/

/*ARSUSED*/
static void
do_receive(pan_upcall_p upcall, pan_msg_p message)
{
    if (request_size > 0) {
	pan_msg_pop(message, request_size, alignof(char));
    }
    pan_msg_empty(message);
    if (reply_size > 0) {
	pan_msg_push(message, reply_size, alignof(char));
    }
    pan_rpc_reply(upcall, message);
}




/*
 * receiver:
 *                 Initializes the RPC layer. The upcall handler
 *                 (do_receive) is registered.
 */

static void
receiver(void)
{
    pan_rpc_register(do_receive);
}


/*ARGSUSED*/
static void
stop_upcall(void *data)
{
    int i;

    --n_stops;
    if (pan_my_pid() == 0) {
	if (n_stops == 0) {
	    for (i = 1; i < pan_nr_platforms(); i++) {
		pan_comm_unicast_small(i, stop_nsap, NULL);
	    }
	}
    }

    if (n_stops == 0) {
	pan_mutex_lock(sync_lock);
	pan_cond_signal(sync_cond);
	pan_mutex_unlock(sync_lock);
    }
}


/*
 * main:
 *                 Startup and shutdown
 */

int
main(int argc, char *argv[])
{
    int i;
    int j;
    int server;
    pan_time_p t;
#ifdef POLL_ON_WAIT
    int poll = FALSE;
#endif

    pan_init(&argc, argv);

    t = pan_time_create();

    if (argc < 2){
	fprintf(stderr, "Usage: %s [-p <reply_size>] [-q <request_size>] <nr_msgs>\n", argv[0]);
	exit(1);
    }


    for (i = 1; i < argc; i++) {
#ifdef AMOEBA
	if (strcmp(argv[i], "-core") == 0) {
	    pan_sys_dump_core();
	} else
#endif
	if (strcmp(argv[i], "-p") == 0) {
	    reply_size = atoi(argv[i+1]);
	    for (j = i; j < argc; j++) {
		argv[j] = argv[j+2];
	    }
	    argc -= 2;
	    --i;
	} else if (strcmp(argv[i], "-q") == 0) {
	    request_size = atoi(argv[i+1]);
	    for (j = i; j < argc; j++) {
		argv[j] = argv[j+2];
	    }
	    argc -= 2;
	    --i;
#ifdef POLL_ON_WAIT
	} else if (strcmp(argv[i], "-poll") == 0) {
	    poll = TRUE;
	    for (j = i; j < argc; j++) {
		argv[j] = argv[j+1];
	    }
	    argc -= 1;
	    --i;
#endif
	} else if (i == 1) {
	    nrmsg = atoi(argv[i]);
	} else {
	    fprintf(stderr, "Unknown arg: \"%s\" -- ignored\n", argv[i]);
	}
    }

    sync_lock = pan_mutex_create();
    sync_cond = pan_cond_create(sync_lock);

    stop_nsap = pan_nsap_create();
    pan_nsap_small(stop_nsap, stop_upcall, 0, PAN_NSAP_UNICAST);

    if (pan_my_pid() == 0) {
	n_stops = pan_nr_platforms() - 1;
    } else {
	n_stops = 1;
    }


    server = 0;

    pan_mp_init();
    pan_rpc_init();

#ifdef POLL_ON_WAIT
    if (poll) {
	pan_rpc_poll_reply();
    }
#endif

    if (pan_my_pid() == server) {
	receiver();
    }

    pan_start();

    if (pan_my_pid() != server) {
	client(server);
    } else {
	printf("%2d: server\n", pan_my_pid());
    }


    pan_mutex_lock(sync_lock);
    while (n_stops > 0) {
	pan_cond_wait(sync_cond);
    }
    pan_mutex_unlock(sync_lock);

    /* Wait for final messages */
    /*
    pan_time_d2t(t, 5.0);
    pan_sleep(t);
    */

    pan_rpc_end();

    pan_nsap_clear(stop_nsap);
    pan_cond_clear(sync_cond);
    pan_mutex_clear(sync_lock);

    pan_time_clear(t);

    pan_end();

    return 0;
}

