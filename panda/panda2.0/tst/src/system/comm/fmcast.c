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
 * fmcast:		test sending and receiving of multicast fragments by
 *			sender 1. If unreliable comm, all receivers ack.
 *			No assembly is done at receiver's.
 */


#define		TIMEOUT		1.0	/* sec */


/* Message types: */


typedef struct DATA_HDR_T {
    int		sender;
#ifndef RELIABLE_MULTICAST
    int		ackno;
#endif
} hdr_t, *hdr_p;


static pan_nsap_p	mcast_nsap;

static int		mcast_me;		/* my cpu */
static int		mcast_nr;		/* tot nr of cpus */

static pan_pset_p	all;


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
static int	data_rcvd	= 0;
static int	data_retrials	= 0;
static int	retrial_rcvd	= 0;
static int	ack_sent	= 0;
static int	ack_rcvd	= 0;

#define STATINC(stat)	((stat)++)

#else		/* STATISTICS */

#define STATINC(stat)

#endif		/* STATISTICS */


/* Monitor for communication between sender and receiver thread. */

static pan_mutex_p	data_lock;

static int		nr_multicasts;	/* total # of data msgs */
static int		nr_data;	/* current # of data msgs */
static pan_cond_p	data_done;	/* CV that all data msgs have arrived */


#ifndef RELIABLE_MULTICAST

static int		rcved_frag;

static pan_nsap_p	ack_nsap;
static int	       *rcved_ack;
static pan_mutex_p	ack_lock;
static pan_cond_p	ack_rcved;

#endif


static void
usage(char *progname)
{
    printf("%s [<cpu_id> <ncpus>] <nr_multicasts> <msg_size>\n",
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



#ifndef RELIABLE_MULTICAST

static int
all_acked(int ack_no)
{
    int i;

    for (i = 0; i < mcast_nr; i++) {
	if (rcved_ack[i] != ack_no) {
	    return 0;
	}
    }

    return 1;
}


static void
rcve_ack(void *data)
{
    hdr_p hdr = (hdr_p)data;

#ifndef NDEBUG
    if (verbose >= 60) {
	printf("%2d:       receive ack sender %d ackno %d\n",
	       mcast_me, hdr->sender, hdr->ackno);
    }
#endif
    pan_mutex_lock(ack_lock);
    STATINC(ack_rcvd);
    if (hdr->ackno == rcved_ack[hdr->sender] + 1) {
	rcved_ack[hdr->sender]++;
	if (all_acked(hdr->ackno)) {
	    pan_cond_signal(ack_rcved);
	}
    }
    pan_mutex_unlock(ack_lock);
}


static void
init_ack_monitor(void)
{
    int i;

    ack_nsap = pan_nsap_create();
    pan_nsap_small(ack_nsap, rcve_ack, sizeof(hdr_t), PAN_NSAP_MULTICAST);

    ack_lock = pan_mutex_create();
    ack_rcved = pan_cond_create(ack_lock);

    rcved_ack = pan_malloc(mcast_nr * sizeof(int));
    for (i = 0; i < mcast_nr; i++) {
	rcved_ack[i] = -1;
    }
}


static void
clear_ack_monitor(void)
{
    pan_free(rcved_ack);

    pan_cond_clear(ack_rcved);
    pan_mutex_clear(ack_lock);

    pan_nsap_clear(ack_nsap);
}

#endif



static void
rcve_frag(pan_fragment_p frgm)
{
    hdr_p hdr;
#ifndef RELIABLE_MULTICAST
    int   sender;
#endif

    hdr = pan_fragment_header(frgm);

#ifndef NDEBUG
    if (verbose >= 40) {
	printf("%2d: rcve data frgm %d <sender %d>\n", mcast_me, nr_data,
		hdr->sender);
    }
#endif

#ifndef RELIABLE_MULTICAST
    sender = hdr->sender;
    hdr->sender = mcast_me;
    STATINC(ack_sent);
    pan_comm_unicast_small(sender, ack_nsap, hdr);

#ifndef NDEBUG
    if (verbose >= 60) {
	printf("%2d:       send ack sender %d ackno %d\n",
	       mcast_me, hdr->sender, hdr->ackno);
    }
#endif

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
    if (nr_data == nr_multicasts) {	/* Time to stop? */
	pan_cond_signal(data_done);
    }
    pan_mutex_unlock(data_lock);
}


static void
multicast_reliable(pan_pset_p dest, pan_fragment_p frgm, int ackno)
{

#ifdef RELIABLE_MULTICAST

    STATINC(data_sent);

    pan_comm_multicast_fragment(dest, frgm);

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

    hdr = pan_fragment_header(frgm);
    hdr->ackno = ackno;

    STATINC(data_sent);

    do {
	if (! first_pass) {
	    pan_mutex_unlock(ack_lock);

#ifndef NDEBUG
	    if (verbose >= 25) {
		printf("%2d: resend multicast %d size %d\n",
		       mcast_me, ackno, msg_size);
	    }
#endif
	    STATINC(data_retrials);
	} else {
	    first_pass = 0;
	}
	pan_comm_multicast_fragment(dest, frgm);
	pan_time_get(t);
	pan_time_add(t, timeout);
	pan_mutex_lock(ack_lock);
	if (! all_acked(ackno)) {
#ifdef POLL_ON_WAIT
	    if (poll_on_sync) {
		static pan_time_p now = NULL;
		if (now == NULL) now = pan_time_create();
		do {
		    pan_mutex_unlock(ack_lock);
		    pan_poll();
		    pan_time_get(now);
		    pan_mutex_lock(ack_lock);
		} while (! all_acked(ackno) && pan_time_cmp(now, t) < 0);
	    } else {
		pan_cond_timedwait(ack_rcved, t);
	    }
#else
	    pan_cond_timedwait(ack_rcved, t);
#endif
	}
    } while (! all_acked(ackno));
    pan_mutex_unlock(ack_lock);

#endif
}


static void
do_mcast(void)
{
    int            i;
    pan_msg_p      msg;
    pan_fragment_p frgm;
    hdr_p          hdr;

    msg = pan_msg_create();
    pan_msg_push(msg, msg_size, alignof(char));
    frgm = pan_msg_fragment(msg, mcast_nsap);

    for (i = 0; i < nr_multicasts; i++) {

	hdr = pan_fragment_header(frgm);
	hdr->sender = mcast_me;

#ifndef NDEBUG
	if (verbose >= 25) {
	    printf("%2d: send multicast %d size %d\n",
		   mcast_me, i, msg_size);
	}
#endif

	multicast_reliable(all, frgm, i);
    }

    pan_msg_clear(msg);
}




static void
init_data_monitor(void)
{
    mcast_nsap = pan_nsap_create();
    pan_nsap_fragment(mcast_nsap, rcve_frag, sizeof(hdr_t), PAN_NSAP_MULTICAST);

    data_lock     = pan_mutex_create();
    data_done     = pan_cond_create(data_lock);
    nr_data       = 0;

#ifndef RELIABLE_MULTICAST
    rcved_frag    = -1;
#endif
}


static void
clear_data_monitor(void)
{
    pan_cond_clear(data_done);
    pan_mutex_clear(data_lock);

    pan_nsap_clear(mcast_nsap);
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

    mcast_me = pan_my_pid();
    mcast_nr = pan_nr_platforms();

    if (mcast_me == mcast_nr) {		/* dedicated sequencer case. Abort */
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
	    case 0: nr_multicasts = atoi(argv[i]);
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

    all = pan_pset_create();
    pan_pset_fill(all);

    init_data_monitor();
#ifndef RELIABLE_MULTICAST
    init_ack_monitor();
#endif

    pan_start();

    if (verbose >= 1) {
	printf("%2d: join the game...\n", mcast_me);
    }

    pan_time_get(start);

    if (mcast_me == mcast_nr - 1) {
	do_mcast();
    } else {
	pan_mutex_lock(data_lock);
	while (nr_data < nr_multicasts) {
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
	    mcast_me, pan_time_t2d(stop), 1, nr_multicasts);
    pan_time_div(stop, nr_multicasts);
    printf("%2d:      per tick    : %f s\n",
	    mcast_me, pan_time_t2d(stop));

    pan_time_clear(start);
    pan_time_clear(stop);

#ifndef RELIABLE_MULTICAST
    clear_ack_monitor();
#endif
    clear_data_monitor();

    pan_pset_clear(all);

    pan_end();

#ifndef NDEBUG
    if (statistics) {
	printf("%2d: Sent  %5s %5s %5s Rcvd  %5s %5s %5s\n",
		mcast_me, "data", "retry", "ack", "data", "retry", "ack");
	printf("%2d:       %5d %5d %5d       %5d %5d %5d\n",
		mcast_me, data_sent, data_retrials, ack_sent,
		data_rcvd, retrial_rcvd, ack_rcvd);
    }
#endif		/* STATISTICS */

    return 0;
}
