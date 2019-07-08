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
static int		n_servers = 1;
static int		request_size = 0;
static int		reply_size = 0;

static pan_mutex_p	sync_lock;
static pan_cond_p	sync_cond;

static int		server_port;


typedef enum MP_TYPE_T {
    MP_DATA,
    MP_STOP
} mp_type_t, *mp_type_p;


typedef struct MP_HDR_T {
    mp_type_t	type;
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
    hdr->sender = me;
    hdr->type   = MP_DATA;
    reply = pan_msg_create();

    pan_time_get(start);

    for (i = 0; i < nrmsg; i++){
	rcve_ticket = pan_mp_receive_message(DIRECT_MAP, reply, MODE_SYNC2);
	hdr->ticket = rcve_ticket;
	send_ticket = pan_mp_send_message(server, server_port, msg, MODE_SYNC2);
	pan_mp_finish_receive(rcve_ticket);
	pan_mp_finish_send(send_ticket);
	pan_msg_empty(reply);
    }

    pan_time_get(stop);
    pan_time_sub(stop, start);

    printf("%2d: %d rpcs: %f s; = ",
	    me, nrmsg, pan_time_t2d(stop));
    pan_time_div(stop, nrmsg);
    throughput = (request_size + reply_size) / (1048576 * pan_time_t2d(stop));
    printf("%f; thr = %f MB/s\n", pan_time_t2d(stop), throughput);

    pan_msg_empty(msg);
    hdr = mp_hdr_push(msg);
    hdr->type = MP_STOP;
    for (i = 0; i < n_servers; i++) {
	pan_mp_send_message(server, server_port, msg, MODE_SYNC);
    }

    pan_time_clear(t);
    pan_msg_clear(msg);
    pan_msg_clear(reply);
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
}


static void
server_daemon(void *arg)
{
    pan_msg_p msg;
    int       stop_count = pan_nr_platforms() - 1;
    mp_hdr_p  hdr;
    int       sender;
    int       ticket;
    int       n = 0;

    msg = pan_msg_create();
    do {
	pan_mp_receive_message(server_port, msg, MODE_SYNC);

	hdr = mp_hdr_pop(msg);
	sender = hdr->sender;
	ticket = hdr->ticket;

	if (hdr->type == MP_STOP) {
	    if (--stop_count == 0) {
		break;
	    } else {
		continue;
	    }
	}

	++n;
	pan_msg_empty(msg);
	if (reply_size > 0) {
	    pan_msg_push(msg, reply_size, alignof(char));
	}

	pan_mp_send_message(sender, ticket, msg, MODE_SYNC);
	pan_msg_empty(msg);
    } while (TRUE);

    printf("S%d: serviced %d msgs\n", me, n);
    pan_msg_clear(msg);
    pan_thread_exit();
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
    pan_thread_p *server_threads;
#ifdef POLL_ON_WAIT
    int poll = FALSE;
#endif

    pan_init(&argc, argv);

    me = pan_my_pid();

    t = pan_time_create();

    if (argc < 2){
	fprintf(stderr, "Usage: %s [-p <reply_size>] [-q <request_size>] [-s server-threads] <nr_msgs>\n", argv[0]);
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
	} else if (strcmp(argv[i], "-s") == 0) {
	    n_servers = atoi(argv[i+1]);
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

	server_threads = pan_malloc(n_servers * sizeof(pan_thread_p));

	for (i = 0; i < n_servers; i++) {
	    server_threads[i] = pan_thread_create(server_daemon, NULL, 0,
						 pan_thread_maxprio(), 0);
	}

	for (i = 0; i < n_servers; i++) {
	    pan_thread_join(server_threads[i]);
	    pan_thread_clear(server_threads[i]);
	}

	pan_free(server_threads);
    }

    pan_mp_end();

    pan_cond_clear(sync_cond);
    pan_mutex_clear(sync_lock);

    pan_time_clear(t);

    pan_end();

    return 0;
}
