#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "pan_sys.h"


#ifdef AMOEBA
extern void pan_sys_dump_core(void);
#endif


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
 * ucast_pingpong:	test sending and receiving of unicast fragments by
 *			sender 1, receiver 0. If unreliable comm, 1 acks.
 *			No assembly is done at receiver's.
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

static int      msg_size = 0;		/* max msg size */

#ifndef NDEBUG
static int	verbose = 1;		/* verbosity of everything */
static int	statistics = FALSE;	/* print statistics? */
#endif
#ifdef POLL_ON_WAIT
static int    	poll_on_sync = FALSE;	/* poll during wait? */
#endif


#ifndef NDEBUG

static int	data_sent	= 0;
static int	data_retrials	= 0;
static int	data_rcvd	= 0;
static int	retrial_rcvd	= 0;
static int	ack_sent	= 0;
static int	ack_rcvd	= 0;

#define STATINC(stat)	((stat)++)

#else		/* STATISTICS */

#define STATINC(stat)

#endif		/* STATISTICS */



/* Monitor for communication between sender and receiver thread. */

static pan_mutex_p	data_lock;

static int		nr_unicasts;	/* total # of data msgs */
static int		nr_data;	/* current # of data msgs */
static pan_cond_p	data_done;	/* CV that all data msgs have arrived */


#ifndef RELIABLE_UNICAST

static int		rcved_frag;

static pan_nsap_p	ack_nsap;
static int		rcved_ack;
static pan_mutex_p	ack_lock;
static pan_cond_p	ack_rcved;

#endif


static void
usage(char *progname)
{
    printf("%s [<cpu_id> <ncpus>] <nr_unicasts> <msg_size> [options]\n",
	   progname);
    printf("options:\n");
#ifdef AMOEBA
    printf("   -core\t: dump core on abort\n");
#endif
#ifdef POLL_ON_WAIT
    printf("   -poll\t: poll during wait() in sync send\n");
#endif
#ifndef NDEBUG
    printf("   -statistics\t: print msg statistics\n");
    printf("   -v <n>\t: verbose level              (default 1)\n");
#endif
}



#ifndef RELIABLE_UNICAST

static void
rcve_small(void *data)
{
    int new_ack = *(int *)data;

    pan_mutex_lock(ack_lock);

    STATINC(ack_rcvd);

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

    hdr = pan_fragment_header(frgm);

#ifndef NDEBUG
    if (verbose >= 40) {
	printf("%2d: rcve data frgm %d <sender %d>\n", ucast_me, nr_data,
		hdr->sender);
    }
#endif

#ifndef RELIABLE_UNICAST
    STATINC(ack_sent);

    pan_comm_unicast_small(hdr->sender, ack_nsap, &hdr->ackno);

    if (rcved_frag + 1 == hdr->ackno) {
	STATINC(data_rcvd);
	rcved_frag++;
    } else {
	STATINC(retrial_rcvd);
	return;		/* Fragment already received (or out of order) */
    }
#endif

    pan_mutex_lock(data_lock);
    ++nr_data;
    if (nr_data == nr_unicasts) {	/* Time to stop? */
	pan_cond_signal(data_done);
    }
    pan_mutex_unlock(data_lock);
}


static void
unicast_reliable(int dest, pan_fragment_p frgm, int ackno)
{

#ifdef RELIABLE_UNICAST

    STATINC(data_sent);

    pan_comm_unicast_fragment(dest, frgm);

#else

    hdr_p             hdr;
    static pan_time_p t = NULL;
    static pan_time_p timeout;
    int               first_pass = 1;

    if (t == NULL) {
	t = pan_time_create();
	timeout = pan_time_create();
	pan_time_d2t(timeout, TIMEOUT);
    }

    STATINC(data_sent);

    hdr = pan_fragment_header(frgm);
    hdr->ackno = ackno;

    do {
	if (! first_pass) {
	    pan_mutex_unlock(ack_lock);
	    STATINC(data_retrials);
	} else {
	    first_pass = 0;
	}

	pan_comm_unicast_fragment(dest, frgm);

	pan_time_get(t);
	pan_time_add(t, timeout);
	pan_mutex_lock(ack_lock);
	if (ackno > rcved_ack) {
#ifdef POLL_ON_WAIT
	    if (poll_on_sync) {
		static pan_time_p now = NULL;
		if (now == NULL) now = pan_time_create();
		do {
		    pan_mutex_unlock(ack_lock);
		    pan_poll();
		    pan_time_get(now);
		    pan_mutex_lock(ack_lock);
		} while (ackno > rcved_ack && pan_time_cmp(now, t) < 0);
	    } else {
		pan_cond_timedwait(ack_rcved, t);
	    }
#else
	    pan_cond_timedwait(ack_rcved, t);
#endif
	}
    } while (ackno > rcved_ack);
    pan_mutex_unlock(ack_lock);

#endif
}


static void
do_ucast(void)
{
    int            i;
    pan_msg_p      msg;
    pan_fragment_p frgm;
    hdr_p          hdr;
    int            sent_frag = 0;

    msg = pan_msg_create();
    pan_msg_push(msg, msg_size, alignof(char));
    frgm = pan_msg_fragment(msg, ucast_nsap);

    for (i = 0; i < nr_unicasts; i++) {

	hdr = pan_fragment_header(frgm);
	hdr->sender = ucast_me;

#ifndef NDEBUG
	if (verbose >= 25) {
	    printf("%2d: send unicast %d to %d size %d\n",
		   ucast_me, i, other, msg_size);
	}
#endif

	unicast_reliable(other, frgm, sent_frag++);
    }

    pan_msg_clear(msg);
}




static void
init_data_monitor(void)
{
    ucast_nsap = pan_nsap_create();
    pan_nsap_fragment(ucast_nsap, rcve_frag, sizeof(hdr_t), PAN_NSAP_UNICAST);

    data_lock     = pan_mutex_create();
    data_done     = pan_cond_create(data_lock);
    nr_data       = 0;

#ifndef RELIABLE_UNICAST
    rcved_frag    = -1;
#endif
}


static void
clear_data_monitor(void)
{
    pan_cond_clear(data_done);
    pan_mutex_clear(data_lock);

    pan_nsap_clear(ucast_nsap);
}



int
main(int argc, char *argv[])
{
    pan_time_p  start;
    pan_time_p  stop;
    int         i;
    int         option;

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
	if (FALSE) {
#ifdef AMOEBA
	} else if (strcmp(argv[i], "-core") == 0) {
	    pan_sys_dump_core();
#endif
#ifdef POLL_ON_WAIT
	} else if (strcmp(argv[i], "-poll") == 0) {
	    poll_on_sync = TRUE;
#endif
#ifndef NDEBUG
	} else if (strcmp(argv[i], "-statistics") == 0) {
	    statistics = TRUE;
	} else if (strcmp(argv[i], "-v") == 0) {
	    ++i;
	    verbose = atoi(argv[i]);
#endif
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

    init_data_monitor();
#ifndef RELIABLE_UNICAST
    init_ack_monitor();
#endif

    pan_start();

#ifndef NDEBUG
    if (verbose >= 1) {
	printf("%2d: join the game...\n", ucast_me);
    }
#endif

    pan_time_get(start);

    if (ucast_me == 1) {
	do_ucast();
    } else {
	pan_mutex_lock(data_lock);
	while (nr_data < nr_unicasts) {
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
    }

    pan_time_get(stop);
    pan_time_sub(stop, start);

    printf("%2d:      total time  : %f s; %d senders each %d ticks\n",
	    ucast_me, pan_time_t2d(stop), 1, nr_unicasts);
    pan_time_div(stop, nr_unicasts);
    printf("%2d:      per tick    : %f s\n",
	    ucast_me, pan_time_t2d(stop));

    pan_time_clear(start);
    pan_time_clear(stop);

#ifndef RELIABLE_UNICAST
    clear_ack_monitor();
#endif
    clear_data_monitor();

    pan_end();

#ifndef NDEBUG
    if (statistics) {
	printf("%2d: Sent  %5s %5s %5s Rcvd  %5s %5s %5s\n",
		ucast_me, "data", "retry", "ack", "data", "retry", "ack");
	printf("%2d:       %5d %5d %5d       %5d %5d %5d\n",
		ucast_me, data_sent, data_retrials, ack_sent,
		data_rcvd, retrial_rcvd, ack_rcvd);
    }
#endif		/* STATISTICS */

    return 0;
}
