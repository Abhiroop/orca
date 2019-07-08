#include "pan_sys.h"
#include "pan_mp.h"
#include "pan_rpc.h"
#include "pan_util.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


/*
   Test rpc implementation using n clients and n servers
*/


/* static variables */
static int            nrmsg;
static int            request_size = 0;
static int            reply_size = 0;

static int           *is_server;
static int           *is_client;
static int           *server_no;
static int            n_servers;
static int            n_clients;
static int            random_msg_size = 0;

static pan_mutex_p    rpc_lock;
static int            services = 0;

static pan_nsap_p     stop_nsap;
static pan_mutex_p    sync_lock;
static pan_cond_p     sync_cond;
static int            n_stops;


/* server has had its portion of messages. Remove its number from the
 * server info */
static void
contract_server_info(int server)
{
    int i;

    is_server[server] = 0;
    for (i = 0; i < n_servers && server_no[i] != server; i++);
    --n_servers;
    for (; i < n_servers; i++) {
	server_no[i] = server_no[i+1];
    }
}


/*
 * client:
 *                 Send nr_msg empty messages
 */
static void
client(void)
{
    pan_time_p start, stop;
    pan_time_p t = pan_time_create();
    pan_msg_p message;
    pan_msg_p reply;
    int i;
    int server;
    int      *sent_to;
    int n_msgs;
    int msg_size;

    srand(pan_my_pid());
    printf("%2d: client, send %d msgs each to %d servers; seed = %d\n",
	   pan_my_pid(), nrmsg, n_servers, pan_my_pid());

    /*
    pan_time_d2t(t, 5.0);
    pan_sleep(t);
    */

    sent_to = calloc(pan_nr_platforms(), sizeof(int));
    start   = pan_time_create();
    stop    = pan_time_create();

    message = pan_msg_create();
    if (request_size > 0) {
	if (random_msg_size) {
	    msg_size = rand() % request_size;
	} else {
	    msg_size = request_size;
	}
	pan_msg_push(message, msg_size, 1);
    }

    pan_time_get(start);

    n_msgs = n_servers * nrmsg;
    for (i = 0; i < n_msgs; i++){
	server = server_no[rand() % n_servers];
	assert(is_server[server]);
	++sent_to[server];

	if (sent_to[server] == nrmsg) {
	    contract_server_info(server);
	}

	pan_rpc_trans(server, message, &reply);
	pan_msg_clear(reply);
	if (request_size > 0 && random_msg_size) {
	    pan_msg_empty(message);
	    msg_size = rand() % request_size;
	    pan_msg_push(message, msg_size, 1);
	}
    }

    pan_time_get(stop);
    pan_time_sub(stop, start);

    pan_comm_unicast_small(0, stop_nsap, NULL);

    printf("%2d: client finished: %g\n", pan_my_pid(), pan_time_t2d(stop));

    /* Wait for delayed replies */
    /*
    pan_time_d2t(t, 3.0);
    pan_sleep(t);
    */

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
    int msg_size;

    pan_mutex_lock(rpc_lock);
    ++services;
    pan_mutex_unlock(rpc_lock);
    pan_msg_empty(message);
    if (reply_size > 0) {
	if (random_msg_size) {
	    msg_size = rand() % reply_size;
	} else {
	    msg_size = reply_size;
	}
	pan_msg_push(message, msg_size, 1);
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
    printf("%2d: server\n", pan_my_pid());

    rpc_lock = pan_mutex_create();
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
    int i_am_server;

    pan_init(&argc, argv);

    t = pan_time_create();

    if (argc < 2){
	fprintf(stderr, "Usage: %s [-s <servers>] [-c <clients>] [-p <reply_size>] [-q <request_size>] <nr_msgs>\n", argv[0]);
	exit(1);
    }

    n_servers = 1;
    n_clients = pan_nr_platforms() - 1;
    if (n_clients == 0) n_clients = 1;

    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-s") == 0) {
	    if (strcmp(argv[i+1], "a") == 0) {
		n_servers = pan_nr_platforms();
	    } else {
		n_servers = atoi(argv[i+1]);
	    }
	    argc -= 2;
	    for (j = i; j < argc; j++) {
		argv[j] = argv[j+2];
	    }
	    --i;
	} else if (strcmp(argv[i], "-c") == 0) {
	    if (strcmp(argv[i+1], "a") == 0) {
		n_clients = pan_nr_platforms();
	    } else {
		n_clients = atoi(argv[i+1]);
	    }
	    for (j = i; j < argc; j++) {
		argv[j] = argv[j+2];
	    }
	    argc -= 2;
	    --i;
	} else if (strcmp(argv[i], "-p") == 0) {
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
	} else if (strcmp(argv[i], "-random") == 0) {
	    random_msg_size = 1;
	    for (j = i; j < argc; j++) {
		argv[j] = argv[j+1];
	    }
	    argc -= 1;
	    --i;
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
	n_stops = n_clients;
    } else {
	n_stops = 1;
    }

    is_server = calloc(pan_nr_platforms(), sizeof(int));
    is_client = calloc(pan_nr_platforms(), sizeof(int));
    server_no = calloc(pan_nr_platforms(), sizeof(int));

    server = 0;
    for (i = 0; i < pan_nr_platforms(); i++) {
	is_server[i] = (i < n_servers);
	if (is_server[i]) {
	    server_no[server] = i;
	    ++server;
	}
	is_client[i] = (i >= pan_nr_platforms() - n_clients);
    }

    /*pan_util_init(); needed for pan_sleep */
    pan_mp_init();
    pan_rpc_init();

    i_am_server = is_server[pan_my_pid()];
    if (i_am_server) {
	receiver();
    }

    pan_start();

    if (is_client[pan_my_pid()]) {
	client();
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

    if (i_am_server) {
	pan_mutex_clear(rpc_lock);
	printf("%2d: serviced %d requests\n", pan_my_pid(), services);
    }

    pan_rpc_end();
    pan_mp_end();
    /* pan_util_end(); needed for pan_sleep */

    pan_nsap_clear(stop_nsap);
    pan_cond_clear(sync_cond);
    pan_mutex_clear(sync_lock);

    pan_time_clear(t);

    pan_end();

    return 0;
}

