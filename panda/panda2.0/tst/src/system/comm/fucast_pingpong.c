#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "pan_sys.h"



#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


#ifdef __GNUC__
#  define INLINE __inline__
#else
#  define INLINE
#endif

/*
 * fucast_pingpong:	test sending and receiving of unicast msgs by
 *			pingponging a fragment, without msg assembly.
 */


#define		TIMEOUT		1.0	/* sec */


/* Message types: */


typedef struct DATA_HDR_T {
    int		sender;
#ifndef RELIABLE_UNICAST
    int		ackno;
#endif
} hdr_t, *hdr_p;


static pan_nsap_p	ucast_nsap;

static int		ucast_me;		/* my cpu */
static int		ucast_nr;		/* tot nr of cpus */

static int              other;		/* cpu to rcve my frags */


/* Global variables that are toggled from command line options */

static int	verbose = 1;		/* verbosity of everything */
static int	statistics = FALSE;	/* print statistics? */
static int	sync_send = FALSE;	/* sync ucast in ucast_msg? */
static int      msg_size = 0;		/* max msg size */
static int    	poll_on_sync = FALSE;	/* poll during wait? */


#ifdef STATISTICS

static int	data_sent = 0;
static int	data_rcvd = 0;

#define STATINC(stat)	((stat)++)

#else		/* STATISTICS */

#define STATINC(stat)

#endif		/* STATISTICS */



/* Monitor for communication between sender and receiver thread. */

static pan_mutex_p	data_lock;

static int		total_data;	/* total # of data msgs */
static int		nr_data;	/* current # of data msgs */
static int		whose_turn;	/* the machine whose turn it is */
static pan_cond_p	data_done;	/* CV that all data msgs have arrived */
static pan_cond_p	my_turn;	/* CV that it is my pong/ping */



#ifndef RELIABLE_UNICAST

static pan_nsap_p	ack_nsap;
static int		rcved_ack;
static pan_mutex_p	ack_lock;
static pan_cond_p	ack_rcved;

#endif


static void
usage(char *progname)
{
    printf("%s [<cpu_id> <ncpus>] <nr_unicasts> <msg_size>\n",
	   progname);
    printf("options:\n");
#ifdef AMOEBA
    printf("   -core\t: dump core on abort\n");
#endif
    printf("   -poll\t: poll during wait() in sync send\n");
    printf("   -statistics\t: print msg statistics\n");
    printf("   -sync\t: sender sync at fragment/assembly layer\n");
    printf("   -v <n>\t: verbose level              (default 1)\n");
}



#ifndef RELIABLE_UNICAST

static void
rcve_small(void *data)
{
    int new_ack = *(int *)data;

    pan_mutex_lock(ack_lock);
    if (new_ack == rcved_ack + 1) {
	rcved_ack++;
	pan_cond_signal(ack_rcved);
    }
    pan_mutex_unlock(ack_lock);
}


static void
init_ack_monitor(void)
{
    ack_nsap = pan_nsap_create();
    pan_nsap_small(ack_nsap, rcve_small, sizeof(int), PAN_NSAP_UNICAST);

    ack_lock = pan_mutex_create();
    ack_rcved = pan_cond_create(ack_lock);
    rcved_ack = -1;
}


static void
clear_ack_monitor(void)
{
    pan_cond_clear(ack_rcved);
    pan_mutex_clear(ack_lock);

    pan_nsap_clear(ack_nsap);
}

#endif



static void
rcve_frag(pan_fragment_p frgm)
{
    hdr_p hdr;

    STATINC(data_rcvd);

    hdr = pan_fragment_header(frgm);

#ifndef NDEBUG
    if (verbose >= 40) {
	printf("%2d: rcve data frgm %d <sender %d>\n", ucast_me, nr_data,
		hdr->sender);
    }
#endif

#ifndef RELIABLE_UNICAST
    pan_comm_unicast_small(hdr->sender, ack_nsap, &hdr->ackno);
#endif

    pan_mutex_lock(data_lock);
    whose_turn = ucast_me;
    pan_cond_signal(my_turn);

    ++nr_data;
    if (nr_data == total_data) {	/* Time to stop? */
	pan_cond_signal(data_done);
    }
    pan_mutex_unlock(data_lock);
}


static void
unicast_reliable(int dest, pan_fragment_p frgm, int ackno)
{
#ifdef RELIABLE_UNICAST
    pan_comm_unicast_fragment(dest, frgm);
#else
    hdr_p          hdr;
    static pan_time_p t = NULL;
    static pan_time_p timeout;

    if (t == NULL) {
	t = pan_time_create();
	timeout = pan_time_create();
	pan_time_d2t(timeout, TIMEOUT);
    }

    hdr = pan_fragment_header(frgm);
    hdr->ackno = ackno;
    do {
	pan_comm_unicast_fragment(dest, frgm);
	pan_time_get(t);
	pan_time_add(t, timeout);
	pan_mutex_lock(ack_lock);
	if (ackno < rcved_ack) {
	    pan_cond_timedwait(ack_rcved, t);
	}
	pan_mutex_unlock(ack_lock);
    } while (ackno < rcved_ack);

#endif
}


static void
do_pingpong(int msg_size, int nr_unicasts)
{
    int            i;
    pan_msg_p      msg;
    pan_fragment_p frgm;
    hdr_p          hdr;

    msg = pan_msg_create();

    pan_msg_push(msg, msg_size, alignof(char));
    frgm = pan_msg_fragment(msg, ucast_nsap);
    hdr  = pan_fragment_header(frgm);
    hdr->sender = ucast_me;

    for (i = 0; i < nr_unicasts; i++) {
	pan_mutex_lock(data_lock);
	while (whose_turn != ucast_me) {
#ifdef POLL_ON_WAIT
	    if (poll_on_sync) {
		pan_mutex_unlock(data_lock);
		pan_poll();
		pan_mutex_lock(data_lock);
	    } else {
		pan_cond_wait(my_turn);
	    }
#else
	    pan_cond_wait(my_turn);
#endif
	}
	whose_turn = -1;
	pan_mutex_unlock(data_lock);

#ifndef NDEBUG
	if (verbose >= 25) {
	    printf("%2d: send unicast %d to %d size %d\n",
		   ucast_me, i, other, msg_size);
	}
#endif
	unicast_reliable(other, frgm, i);

	STATINC(data_sent);
    }

    pan_mutex_lock(data_lock);
    while (nr_data < total_data) {
#ifdef POLL_ON_WAIT
	if (poll_on_sync) {
	    pan_mutex_unlock(data_lock);
	    pan_poll();
	    pan_mutex_lock(data_lock);
	} else {
	    pan_cond_wait(data_done);
	}
#else
	pan_cond_wait(data_done);
#endif
    }
    pan_mutex_unlock(data_lock);

    pan_msg_clear(msg);
}




static void
init_data_monitor(int tot_d)
{
    ucast_nsap = pan_nsap_create();
    pan_nsap_fragment(ucast_nsap, rcve_frag, sizeof(hdr_t), PAN_NSAP_UNICAST);

    data_lock     = pan_mutex_create();
    data_done     = pan_cond_create(data_lock);
    my_turn       = pan_cond_create(data_lock);
    nr_data       = 0;
    whose_turn    = 0;
    total_data    = tot_d;
}


static void
clear_data_monitor(void)
{
    pan_cond_clear(my_turn);
    pan_cond_clear(data_done);
    pan_mutex_clear(data_lock);

    pan_nsap_clear(ucast_nsap);
}



int
main(int argc, char *argv[])
{
    pan_time_p  start;
    pan_time_p  stop;
    int         nr_unicasts = 1;	/* # ucasts sent per sending procs. */
    int         i;
    int         option;
    int         scatter = FALSE;

    pan_init(&argc, argv);

    start = pan_time_create();
    stop = pan_time_create();

    ucast_me = pan_my_pid();
    ucast_nr = pan_nr_platforms();

    if (ucast_me == ucast_nr) {		/* dedicated sequencer case. Abort */
	pan_end();
	return 0;
    }

    option = 0;
    for (i = 1; i < argc; i++) {
#ifdef AMOEBA
	if (strcmp(argv[i], "-core") == 0) {
	    pan_sys_dump_core();
	} else
#endif
	if (strcmp(argv[i], "-poll") == 0) {
	    poll_on_sync = TRUE;
	} else if (strcmp(argv[i], "-scatter") == 0) {
	    scatter = TRUE;
	} else if (strcmp(argv[i], "-statistics") == 0) {
	    statistics = TRUE;
	} else if (strcmp(argv[i], "-sync") == 0) {
	    sync_send = TRUE;
	} else if (strcmp(argv[i], "-v") == 0) {
	    ++i;
	    verbose = atoi(argv[i]);
	} else {
	    switch (option) {
	    case 0: nr_unicasts = atoi(argv[i]);
		    break;
	    case 1: msg_size      = atoi(argv[i]);
		    break;
	    default:
		    printf("Unkown option: %s\n", argv[i]);
		    pan_end();
		    usage(argv[0]);
	    }
	    ++option;
	}
    }

    other = ucast_me + 1;
    if (other == ucast_nr) other = 0;

    init_data_monitor(nr_unicasts);
#ifndef RELIABLE_UNICAST
    init_ack_monitor();
#endif

#ifdef AMOEBA
    pan_sys_dump_core();
#endif

    pan_start();

    if (verbose >= 1) {
	printf("%2d: join the game...\n", ucast_me);
    }

#ifdef NEVER
    if (poll_on_sync) {
	pan_comm_stop_idle();
    }
#endif

    pan_time_get(start);

    do_pingpong(msg_size, nr_unicasts);

    pan_time_get(stop);
    pan_time_sub(stop, start);

    printf("%2d:      total time  : %f s; %d senders each %d ticks\n",
	    ucast_me, pan_time_t2d(stop), ucast_nr, nr_unicasts);
    pan_time_div(stop, ucast_nr * nr_unicasts);
    printf("%2d:      per tick    : %f s\n",
	    ucast_me, pan_time_t2d(stop));

    pan_time_clear(start);
    pan_time_clear(stop);

#ifndef RELIABLE_UNICAST
    clear_ack_monitor();
#endif
    clear_data_monitor();

    pan_end();

    if (statistics) {
#ifdef STATISTICS
	printf("%2d: Sent data   Rcvd data\n",
		ucast_me);
	printf("%2d:     %5d      %5d\n",
		ucast_me, data_sent, data_rcvd);
#endif		/* STATISTICS */
    }

    return 0;
}
