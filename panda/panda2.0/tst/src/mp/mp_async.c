#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "pan_sys.h"
#include "pan_mp.h"


extern void pan_sys_dump_core(void);
extern void pan_mp_poll_reply(void);


#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/*
   Test mp implementation using n clients and n servers
*/


/* static variables */

static int		me;
static int		nrmsg;
static int		request_size = 0;
static int		reply_size = 0;

static pan_nsap_p	stop_nsap;
static pan_mutex_p	sync_lock;
static pan_cond_p	sync_cond;
static int		n_stops;

static int		server_port;


typedef struct MP_HDR_T {
    short int	sender;
    short int	ticket;
} mp_hdr_t, *mp_hdr_p;


static mp_hdr_p
mp_hdr_push(pan_msg_p msg)
{
    return pan_msg_push(msg, sizeof(mp_hdr_t), alignof(mp_hdr_t));
}

static mp_hdr_p
mp_hdr_pop(pan_msg_p msg)
{
    return pan_msg_pop(msg, sizeof(mp_hdr_t), alignof(mp_hdr_t));
}



/*
 * client:
 *                 Send nr_msg empty messages
 */
static void
client(int server)
{
    pan_time_p start, stop;
    pan_time_p t = pan_time_create();
    pan_msg_p  msg;
    pan_msg_p  reply;
    double     throughput;
    int        i;
    mp_hdr_p   hdr;
    int        send_ticket;
    int        rcve_ticket;

    printf("%2d: client, send %d msgs\n", me, nrmsg);

    /*
    pan_time_d2t(t, 5.0);
    pan_sleep(t);
    */

    start   = pan_time_create();
    stop    = pan_time_create();

    msg = pan_msg_create();
    if (request_size > 0) {
	pan_msg_push(msg, request_size, alignof(char));
    }
    hdr = mp_hdr_push(msg);
    reply = pan_msg_create();

    pan_time_get(start);

    for (i = 0; i < nrmsg; i++){
	rcve_ticket = pan_mp_receive_message(DIRECT_MAP, reply, MODE_SYNC2);
	hdr->sender = me;
	hdr->ticket = rcve_ticket;
	send_ticket = pan_mp_send_message(server, server_port, msg, MODE_SYNC2);
	pan_mp_finish_receive(rcve_ticket);
	pan_mp_finish_send(send_ticket);
	pan_msg_empty(reply);
    }

    pan_time_get(stop);
    pan_time_sub(stop, start);

    pan_comm_unicast_small(0, stop_nsap, NULL);

    printf("%2d: %d rpcs: %f s; = ",
	    me, nrmsg, pan_time_t2d(stop));
    pan_time_div(stop, nrmsg);
    throughput = (request_size + reply_size) / (1048576 * pan_time_t2d(stop));
    printf("%f; thr = %f MB/s\n", pan_time_t2d(stop), throughput);

    pan_time_clear(t);
    pan_msg_clear(msg);
    pan_msg_clear(reply);
}



/*
   do_receive:
                   Function called by the RPC module to handle a request
                   msg. It sends a reply back. The reply messsage is
                   cleared by the RPC layer.
*/

/*ARSUSED*/
static void
do_receive(int port, pan_msg_p msg)
{
    mp_hdr_p hdr;
    int      sender;
    int      ticket;

    hdr = mp_hdr_pop(msg);
    sender = hdr->sender;
    ticket = hdr->ticket;

    pan_msg_empty(msg);
    if (reply_size > 0) {
	pan_msg_push(msg, reply_size, alignof(char));
    }

    pan_mp_send_message(sender, ticket, msg, MODE_ASYNC);
}




/*
 * start_send:
 *                 Starts the MP layer. The upcall handler
 *                 (do_receive) is registered.
 */

static void
start_send(void)
{
    server_port = pan_mp_register_map();
    assert(server_port != DIRECT_MAP);
    pan_mp_register_async_receive(server_port, do_receive);
}


/*ARGSUSED*/
static void
stop_upcall(void *data)
{
    int i;

    --n_stops;
    if (me == 0) {
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

    me = pan_my_pid();

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

    if (me == 0) {
	n_stops = pan_nr_platforms() - 1;
    } else {
	n_stops = 1;
    }


    server = 0;

    pan_mp_init();

#ifdef POLL_ON_WAIT
    if (poll) {
	pan_mp_poll_reply();
    }
#endif

    start_send();

    pan_start();

    if (me != server) {
	client(server);
    } else {
	printf("%2d: server\n", me);
    }


    pan_mutex_lock(sync_lock);
    while (n_stops > 0) {
	pan_cond_wait(sync_cond);
    }
    pan_mutex_unlock(sync_lock);

    pan_mp_end();

    pan_nsap_clear(stop_nsap);
    pan_cond_clear(sync_cond);
    pan_mutex_clear(sync_lock);

    pan_time_clear(t);

    pan_end();

    return 0;
}
