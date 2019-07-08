#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "pan_sys.h"

#include "pan_util.h"

/*
 * Test1: test internals of Panda; sending and receiving of msgs.
 * Starting this test is bit of problem. It should be started twice: the server
 * process waits for a msg and sends an acknowledgement on the msg.
 * The client sends msgs and waits for some time for an acknowledgment. If
 * no acknowledgement arrives, it retransmits the msg.
 */


#define		MAXRETRIAL	1000	/* maximum number of retries */
#define		TIMEOUT		2.0
#define		SERVER		0	/* server on platform 1 */
#define 	CLIENT		1	/* client on 2 */

#define CHAR_MASK	0xff

static int  cpu, ncpu;

static pan_mutex_p snd_mutex;
static pan_cond_p snd_cond;
static int  snd_seqno;		/* sequence number of msg that is being
				 * sent */
static int  nr_retrans = 0;	/* # retransmissions */

static pan_mutex_p end_mutex;
static pan_cond_p end_cond;

static int  rcvd_msgs = 0;	/* number of msgs that has been assembled */
static int  rcve_seqno = 0;	/* sequence number of next msg to be received */


static pan_nsap_p global_nsap;	/* our communication channel */
#ifndef RELIABLE_UNICAST
static pan_nsap_p ack_nsap;	/* our acknowledge channel */
#endif

static pan_msg_p  catch;	/* assemble received fragments here */

static int  verbose = 0;


typedef enum FILL_T {
    NO_FILL,
    FILL_FIXED,
    FILL_VAR
} fill_t, *fill_p;


static fill_t	msg_fill = NO_FILL;
static int	check = 0;


typedef struct frag_hdr {
    int frag_seqno;
} frag_hdr_t, *frag_hdr_p;


static int	msg_size;
static int	nr_unicasts;		/* # unicasts per msg_size */


#ifdef RELIABLE_UNICAST


static void
reliable_unicast(
       int       server,
       pan_msg_p msg
)
{
    pan_fragment_p frag;

    frag = pan_msg_fragment(msg, global_nsap);
    do {
	pan_comm_unicast_fragment(server, frag);	/* send to server */
    } while (pan_msg_next(msg) != NULL);
}


#else		/* RELIABLE_UNICAST */


static void
reliable_unicast(
       int       server,
       pan_msg_p msg
)
{
    int         retrial;
    static pan_time_p  t = NULL;
    static pan_time_p  timeout;
    int         seqno;
    pan_fragment_p frag;
    frag_hdr_p  hdr;

    if (t == NULL) {
	t = pan_time_create();
	timeout = pan_time_create();
	pan_time_d2t(timeout, TIMEOUT);
    }

    pan_mutex_lock(snd_mutex);		/* Protect snd_seqno */

    frag = pan_msg_fragment(msg, global_nsap);

    do {
	seqno = snd_seqno;
	hdr = pan_fragment_header(frag);
	hdr->frag_seqno = seqno;

	if (verbose >= 30) {
	    printf("%2d: send frag id %d\n", pan_my_pid(), snd_seqno);
	}

	retrial = 0;
	do {
	    pan_mutex_unlock(snd_mutex);

	    if (retrial == MAXRETRIAL) {
		pan_msg_clear(msg);
		pan_panic("Sending msg %d reliably failed\n", seqno);
	    }
	    ++retrial;

	    if (verbose >= 50) {
		printf("%2d:    ... send out frag id %d\n", pan_my_pid(), snd_seqno);
	    }

	    pan_comm_unicast_fragment(server, frag);	/* send to server */

	    pan_time_get(t);
	    pan_time_add(t, timeout);
	    pan_mutex_lock(snd_mutex);
	    if (seqno + 1 != snd_seqno) {
		pan_cond_timedwait(snd_cond, t);
	    }
	} while (seqno + 1 != snd_seqno);

    } while (pan_msg_next(msg) != NULL);

    pan_mutex_unlock(snd_mutex);

    nr_retrans += retrial - 1;
}


#endif		/* RELIABLE_UNICAST */


static void
plw_receive(pan_fragment_p rcve_msg)
{
    int         seqno;
#ifndef RELIABLE_UNICAST
    frag_hdr_t  ack;
#endif
    frag_hdr_p  hdr;
    int         last_frag;
    char       *data;
    int         j;

    hdr = pan_fragment_header(rcve_msg);
    seqno = hdr->frag_seqno;

    if (verbose >= 30) {
	printf("%2d: rcve frag id %d\n", pan_my_pid(), seqno);
    }

    pan_mutex_lock(end_mutex);

#ifndef RELIABLE_UNICAST
    /* Send acknowledgement. */
    ack.frag_seqno = seqno;
    pan_comm_unicast_small(CLIENT, ack_nsap, &ack);
#endif		/* RELIABLE_UNICAST */

    if (seqno == rcve_seqno) {
	++rcve_seqno;

	last_frag = (pan_fragment_flags(rcve_msg) & PAN_FRAGMENT_LAST);
	pan_msg_assemble(catch, rcve_msg, 0);

	if (last_frag) {

	    if (check) {
		data = pan_msg_look(catch, msg_size, alignof(char));
		if (msg_fill == FILL_FIXED) {
		    for (j = 0; j < msg_size; j++) {
			if (data[j] != 'a')
			    pan_panic("data[%d] = %c", j, data[j]);
		    }
		} else if (msg_fill == FILL_VAR) {
		    for (j = 0; j < msg_size; j++) {
			if (data[j] != (char)((rcvd_msgs + j) & CHAR_MASK))
			    pan_panic("data[%d] = %c", j, data[j]);
		    }
		}
	    }

	    pan_msg_empty(catch);
	    ++rcvd_msgs;
	}
    }

    if (rcvd_msgs == nr_unicasts) {
	pan_cond_broadcast(end_cond);
    }

    pan_mutex_unlock(end_mutex);
}


#ifndef RELIABLE_UNICAST

static void
rcve_ack(void *data)
{
    frag_hdr_p hdr = (frag_hdr_p)data;

    pan_mutex_lock(snd_mutex);
    if (hdr->frag_seqno == snd_seqno) {
	/* Received acknowledgement; wakeup sending thread */
	snd_seqno++;
	pan_cond_signal(snd_cond);
    }
    pan_mutex_unlock(snd_mutex);
}

#endif


static void
sender(void)
{
    pan_time_p  start = pan_time_create();
    pan_time_p  stop = pan_time_create();
    float       throughput;
    char       *data;
    pan_msg_p   msg;
    int         i;
    int         j;

    msg = pan_msg_create();
    data = pan_msg_push(msg, msg_size, alignof(char));

    if (msg_fill == FILL_FIXED) {
	memset(data, 'a', msg_size);
    }

    pan_time_get(start);
    for (i = 0; i < nr_unicasts; i++) {
	data = pan_msg_look(msg, msg_size, alignof(char));
	if (msg_fill == FILL_VAR) {
	    for (j = 0; j < msg_size; j++) {
		data[j] = (char)((i + j) & CHAR_MASK);
	    }
	}
	reliable_unicast(SERVER, msg);
	if (check) {
	    data = pan_msg_look(msg, msg_size, alignof(char));
	    if (msg_fill == FILL_FIXED) {
		for (j = 0; j < msg_size; j++) {
		    if (data[j] != 'a') pan_panic("data[%d] = %c", j, data[j]);
		}
	    } else if (msg_fill == FILL_VAR) {
		for (j = 0; j < msg_size; j++) {
		    if (data[j] != (char)((i + j) & CHAR_MASK))
			pan_panic("data[%d] = %c", j, data[j]);
		}
	    }
	}
    }
    pan_time_get(stop);
    pan_time_sub(stop, start);

    throughput = (msg_size * nr_unicasts) / (1024 * pan_time_t2d(stop));
    printf("%2d: %d Send size %d: %f s. ",
	    cpu, nr_unicasts, msg_size, pan_time_t2d(stop));
    pan_time_div(stop, nr_unicasts);
    printf("; per msg %f, thrp %.1f Kb/s\n",
	    pan_time_t2d(stop), throughput);

    pan_msg_clear(msg);

    printf("%2d: nr retransmissions: %d\n", cpu, nr_retrans);
    pan_time_clear(start);
    pan_time_clear(stop);
}


int
main(
     int argc,
     char *argv[]
)
{
    int  i;
    int  acount;

    if (argc < 3) {
	fprintf(stderr, "usage: %s [<cpu_id> <ncpus>] <nr_unicasts> <msg_size>\n",
		argv[0]);
	exit(1);
    }

    pan_init(&argc, argv);

    cpu = pan_my_pid();
    ncpu = pan_nr_platforms();

    acount = 0;
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-check") == 0) {
	    check = 1;
#ifdef AMOEBA
	} else if (strcmp(argv[i], "-core") == 0) {
	    pan_sys_dump_core();
#endif
	} else if (strcmp(argv[i], "-fill") == 0) {
	    ++i;
	    if (strcmp(argv[i], "fixed") == 0) {
		msg_fill = FILL_FIXED;
	    } else if (strcmp(argv[i], "var") == 0) {
		msg_fill = FILL_VAR;
	    } else {
		fprintf(stderr, "-fill {fixed|var}\n");
	    }
	} else if (strcmp(argv[i], "-v") == 0) {
	    ++i;
	    verbose = atoi(argv[i]);
	} else {
	    if (acount == 0) {
		nr_unicasts = atoi(argv[i]);
	    } else if (acount == 1) {
		msg_size = atoi(argv[2]);
	    } else {
		printf("No such option: %s\n", argv[i]);
	    }
	    ++acount;
	}
    }

    if (acount != 2) {
	fprintf(stderr, "usage: %s [<cpu_id> <ncpus>] <nr_unicasts> <msg_size>\n",
		argv[0]);
	exit(1);
    }

    if (ncpu != 2) {
	pan_panic("# cpus should be 2\n");
    }

    global_nsap = pan_nsap_create();
    pan_nsap_fragment(global_nsap, plw_receive, sizeof(frag_hdr_t),
			PAN_NSAP_UNICAST);

#ifndef RELIABLE_UNICAST
    ack_nsap = pan_nsap_create();
    pan_nsap_small(ack_nsap, rcve_ack, sizeof(frag_hdr_t), PAN_NSAP_UNICAST);
#endif

    snd_mutex = pan_mutex_create();
    snd_cond  = pan_cond_create(snd_mutex);
    snd_seqno = 0;

    end_mutex = pan_mutex_create();
    end_cond  = pan_cond_create(end_mutex);

    catch = pan_msg_create();

    pan_start();

    if (cpu == CLIENT) {
	printf("%2d: starting sending...\n", cpu);
	sender();
    } else if (cpu == SERVER) {
	printf("%2d: server ready\n", cpu);
	pan_mutex_lock(end_mutex);
	while (rcvd_msgs < nr_unicasts) {
	    pan_cond_wait(end_cond);
	}
	pan_mutex_unlock(end_mutex);
    } else {
	pan_panic("Illegal cpu nr %d...\n", cpu);
    }

    pan_mutex_clear(snd_mutex);
    pan_cond_clear(snd_cond);
    pan_mutex_clear(end_mutex);
    pan_cond_clear(end_cond);


#ifndef RELIABLE_UNICAST
    pan_nsap_clear(ack_nsap);
#endif
    pan_nsap_clear(global_nsap);

    pan_msg_clear(catch);

    pan_end();
    return 0;
}
