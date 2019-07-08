#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "pan_sys.h"

#include "pan_timer.h"

#include "pan_util.h"
extern double stdev(double sumsqr, double sum, int n);


#include "mcast_msg.h"

#ifdef __GNUC__
#  define INLINE __inline__
#else
#  define INLINE
#endif

#ifndef FALSE
#  define FALSE	0
#endif
#ifndef TRUE
#  define TRUE	1
#endif

/*
 * mcast: test sending and receiving of multicast msgs.
 */


#define		MAX_RETRIAL	15	/* maximum number of retries */
#define		QUIT_TIMEOUT	10.0	/* sec */


/* Message types: */

typedef enum MSG_TYPE {
    DATA,
    REPORT
} msg_type_t, *msg_type_p;


typedef struct DATA_HDR_T {
    msg_type_t type;
    int        size;
    int        sender;
} hdr_t, *hdr_p;


static char *
msg_type_asc(msg_type_t tp)
{
    switch (tp) {
    case DATA:		return "DATA";
    case REPORT:	return "REPORT";
    }
    return NULL;	/*NOTREACHED*/
}
    

static double	sum_time;		/* sum of times needed */
static double	sq_sum_time;		/* sum of squares of times needed */
static int	nsender;		/* nr of sending processes */
static int	cpu;			/* my cpu */
static int	ncpu;			/* tot nr of cpus */



typedef enum MSG_FILL_STYLE_T {
    NO_FILL,
    FIXED_FILL,
    VAR_FILL
} msg_fill_style_t, *msg_fill_style_p;

/* Global variables that are toggled from command line options */

static int	verbose = 1;		/* verbosity of everything */
static msg_fill_style_t fill_msg = NO_FILL;	/* Fill msgs? */
static int	check_msg = FALSE;	/* Check their contents? */
static int	statistics = FALSE;	/* print statistics? */
static int	sync_send = TRUE;	/* sync mcast? */
static int      msg_size = 0;		/* max msg size */
static int    	poll_on_sync = FALSE;	/* poll during wait? */


#ifndef NOSTATISTICS

static int	data_sent = 0;
static int	data_rcvd = 0;
static int	report_sent = 0;
static int	report_rcvd = 0;

#define STATINC(stat)	((stat)++)

#else		/* STATISTICS */

#define STATINC(stat)

#endif		/* STATISTICS */

static pan_pset_p all;

static int       *is_sender_memo;
static int        reporter;


/* Monitor for communication between sender and receiver thread. */

static pan_mutex_p	data_lock;

static int		total_data;	/* total # of data msgs */
static int		nr_data;	/* current # of data msgs */
static pan_cond_p	data_done;	/* CV that all data msgs have arrived */


/* Monitor for sync barrier between senders */

static pan_mutex_p	sync_lock;

static int		report_seqno;		/* seq no of current report */
static int	       *report_vector;		/* sync from other senders */
static pan_cond_p       reports_arrived;	/* CV that nr_reports == ncpu */
static int              nr_reports;
static int              total_reports;





/*----- Module for sync msg ----------------------------------------------*/

typedef struct REPORT_MSG {
    double      my_time;
    int         seqno;
} report_msg_t, *report_msg_p;


static report_msg_p
report_msg_push(pan_msg_p msg, double msc, int ssq)
{
    report_msg_p s;

    s = pan_msg_push(msg, sizeof(report_msg_t), alignof(report_msg_t));
    s->my_time = msc;
    s->seqno   = ssq;
    return s;
}


static report_msg_p
report_msg_pop(pan_msg_p msg)
{
    return pan_msg_pop(msg, sizeof(report_msg_t), alignof(report_msg_t));
}

/*----- end of Module for sync msg ---------------------------------------*/



static void
hdr_push(pan_msg_p msg, msg_type_t tp, int size, int sender)
{
    hdr_p hdr;

    hdr = pan_msg_push(msg, sizeof(hdr_t), alignof(hdr_t));
    hdr->type   = tp;
    hdr->size   = size;
    hdr->sender = sender;
}



static hdr_p
hdr_pop(pan_msg_p msg)
{
    return pan_msg_pop(msg, sizeof(hdr_t), alignof(hdr_t));
}


static int
is_sender(int cpu)
{
    return is_sender_memo[cpu];
}




static void
send_report(double my_time)
{
    pan_msg_p   msg;

    msg = pan_msg_create();
    report_msg_push(msg, my_time, report_seqno);
    ++report_seqno;
    hdr_push(msg, REPORT, 0, cpu);

    if (verbose >= 25) {
	printf("%2d: send report\n", cpu);
    }
    					/* Reliably mcast our REPORT */
    mcast_msg(msg);
    STATINC(report_sent);
    pan_msg_clear(msg);
}


static INLINE void
handle_report(pan_msg_p msg, hdr_p hdr)
{
    report_msg_p report_body;

    STATINC(report_rcvd);

    report_body = report_msg_pop(msg);

    pan_mutex_lock(sync_lock);	/* Protect sync_seqno, nr_sync */
    assert(report_vector[hdr->sender] == report_body->seqno - 1);

    report_vector[hdr->sender] = report_body->seqno;

    if (verbose >= 5 && cpu == reporter) {
	printf("%2d: updating. Get my_time = %f from %d\n",
		cpu, report_body->my_time, hdr->sender);
    }
    sum_time += report_body->my_time;
    sq_sum_time += report_body->my_time * report_body->my_time;

    ++nr_reports;
    if (nr_reports == total_reports) {
	pan_cond_signal(reports_arrived);
    }

    pan_mutex_unlock(sync_lock);
}


static INLINE void
handle_data(pan_msg_p msg, hdr_p hdr)
{
    int             i;
    int             size;
    char           *data;

    STATINC(data_rcvd);

    if (check_msg) {
	size = hdr->size;
	assert(size == msg_size);
	    if (size > 0) {
	    data = pan_msg_look(msg, size, alignof(char));
	    switch (fill_msg) {
	    case FIXED_FILL:
		for (i = 0; i < size; i++) {
		    assert(data[i] == 'a' + hdr->sender - (ncpu - nsender));
		}
		break;
	    case VAR_FILL:
		for (i = 0; i < size; i++) {
		    assert(data[i] == ' ' + (i & 63));
		}
		break;
	    default:
		;
	    }
	}
    }

    pan_mutex_lock(data_lock);
    if (++nr_data == total_data) {	/* Time to stop? */
	pan_cond_signal(data_done);
    }
    pan_mutex_unlock(data_lock);
}




static void
receive(pan_msg_p msg)
{
    hdr_p hdr;

    hdr = hdr_pop(msg);

    switch (hdr->type) {

    case REPORT:
	handle_report(msg, hdr);
	break;

    case DATA:
	handle_data(msg, hdr);
	break;

    default:
	pan_panic("Impossible switch case %d in plw_receive\n", hdr->type);

    }

    hdr_push(msg, hdr->type, hdr->size, hdr->sender);
}



static void
send_msgs(int msg_size, int nr_multicasts)
{
    char       *data;
    int         i;
    pan_msg_p   msg;

    msg = pan_msg_create();
    data = pan_msg_push(msg, msg_size, alignof(char));
    switch (fill_msg) {
    case FIXED_FILL:
	memset(data, 'a' + cpu - (ncpu - nsender), (size_t)msg_size);
	break;
    case VAR_FILL:
	for (i = 0; i < msg_size; i++) {
	    data[i] = ' ' + (i & 63);
	}
	break;
    default:
	;
    }
    hdr_push(msg, DATA, msg_size, cpu);

    for (i = 0; i < nr_multicasts; i++) {
	if (verbose >= 25) {
	    printf("%2d: send multicast %d of size %d\n", cpu, i, msg_size);
	}
	mcast_msg(msg);

	/* pan_poll(); */

	STATINC(data_sent);
    }

    pan_msg_clear(msg);
}



static void
init_data_monitor(int tot_d)
{
    data_lock     = pan_mutex_create();
    data_done     = pan_cond_create(data_lock);
    nr_data       = 0;
    total_data    = tot_d;
}


static void
init_sync_monitor(int n)
{
    int i;

    sync_lock       = pan_mutex_create();
    reports_arrived = pan_cond_create(sync_lock);
    nr_reports      = 0;
    total_reports   = n;
    report_vector   = malloc(ncpu * sizeof(int));
    for (i = 0; i < ncpu; i++) {
	report_vector[i] = -1;
    }
}


static void
clear_data_monitor(void)
{
    pan_cond_clear(data_done);
    pan_mutex_clear(data_lock);
}


static void
clear_sync_monitor(void)
{
    free(report_vector);
    pan_cond_clear(reports_arrived);
    pan_mutex_clear(sync_lock);
}


static void
init_sender_memo(int scatter, int nsender)
{
    int i;
    int n;

    is_sender_memo = calloc(ncpu, sizeof(int));

    if (scatter) {
	n = 0;
	srand(nsender * ncpu);

	if (nsender < ncpu / 2) {
	    while (n < nsender) {
		i = rand();
		i = i / (RAND_MAX / ncpu);
		if (i < ncpu && ! is_sender_memo[i]) {
		    is_sender_memo[i] = TRUE;
		    ++n;
		}
	    }
	} else {
	    for (i = 0; i < ncpu; i++) {
		is_sender_memo[i] = TRUE;
	    }
	    while (n < ncpu - nsender) {
		i = rand() / (RAND_MAX / ncpu);
		if (i < ncpu && is_sender_memo[i]) {
		    is_sender_memo[i] = FALSE;
		    ++n;
		}
	    }
	}
    } else {
	for (i = 0; i < ncpu; i++) {
	    is_sender_memo[i] = (i >= ncpu - nsender);
	}
    }

    reporter = ncpu - 1;
    while (!is_sender_memo[reporter] && reporter >= 0)
	--reporter;
}


static void
clear_sender_memo(void)
{
    free(is_sender_memo);
}


static void
usage(char *progname)
{
    printf("%s [<cpu_id> <ncpus>] <nr_multicasts> <msg_size>\n",
	   progname);
    printf("options:\n");
    printf("   -check\t: check msg contents (meaningless without -fill)\n");
    printf("   -fill none\t\t: do not fill msg with characters\n");
    printf("   -fill fixed\t\t: fill msg with 1 character\n");
    printf("   -fill var\t\t: fill msg with different characters\n");
    printf("   -s <senders>\t: number of senders	(default 1)\n");
    printf("   -scatter\t: scatter senders\n");
    printf("   -statistics\t: print msg statistics\n");
    printf("   -async\t: don't await receipt of previous msg\n");
#ifdef POLL_ON_WAIT
    printf("   -poll\t: poll during wait() in sync send\n");
#endif
    printf("   -v <n>\t: verbose level              (default 1)\n");
}


int
main(
     int argc,
     char **argv
)
{
    pan_time_p  start = pan_time_create();
    pan_time_p  stop = pan_time_create();
    int         nr_multicasts = 1;	/* # mcasts sent per sending procs. */
    int         i;
    int         n_attempts;
    int         option;
    pan_time_p  t = pan_time_create();
    pan_time_p  timeout = pan_time_create();
    int         scatter = FALSE;
    double      std_dt;
    double      throughput;
    double      std_thr;

    pan_init(&argc, argv);

    cpu = pan_my_pid();
    ncpu = pan_nr_platforms();

    if (cpu == ncpu) {			/* dedicated sequencer case. Abort */
	pan_end();
	return 0;
    }

    pan_time_d2t(timeout, QUIT_TIMEOUT);

    nsender = 1;

    option = 0;
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-fill") == 0) {
	    ++i;
	    if (strcmp(argv[i], "none") == 0) {
		fill_msg = NO_FILL;
	    } else if (strcmp(argv[i], "fixed") == 0) {
		fill_msg = FIXED_FILL;
	    } else if (strcmp(argv[i], "var") == 0) {
		fill_msg = VAR_FILL;
	    } else {
		printf("No such fill style: %s\n", argv[i]);
		pan_end();
		usage(argv[0]);
	    }
#ifdef POLL_ON_WAIT
	} else if (strcmp(argv[i], "-poll") == 0) {
	    poll_on_sync = TRUE;
#endif
	} else if (strcmp(argv[i], "-check") == 0) {
	    check_msg = TRUE;
	} else if (strcmp(argv[i], "-s") == 0) {
	    ++i;
	    nsender = atoi(argv[i]);
	} else if (strcmp(argv[i], "-scatter") == 0) {
	    scatter = TRUE;
	} else if (strcmp(argv[i], "-statistics") == 0) {
	    statistics = TRUE;
	} else if (strcmp(argv[i], "-async") == 0) {
	    sync_send = FALSE;
	} else if (strcmp(argv[i], "-v") == 0) {
	    ++i;
	    verbose = atoi(argv[i]);
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

    if (nsender > ncpu || nsender <= 0) {
	pan_panic("Illegal nsender value: %d\n", nsender);
    }

    all = pan_pset_create();
    pan_pset_fill(all);

    mcast_msg_start(all, receive, sync_send, poll_on_sync, verbose);

    init_sender_memo(scatter, nsender);
    init_data_monitor(nsender * nr_multicasts);
    init_sync_monitor(ncpu);

    sum_time = 0.0;
    sq_sum_time = 0.0;

    pan_sys_dump_core();

    pan_start();

#ifdef NEVER
    if (poll_on_sync) {
	pan_comm_stop_idle();
    }
#endif

    pan_time_get(start);

    if (is_sender(cpu)) {	/* i am sender */
	if (verbose >= 1) {
	    fprintf(stderr, "%2d: is sender\n", cpu);
	}

	send_msgs(msg_size, nr_multicasts);

    } else {
	if (verbose >= 1) {
	    fprintf(stderr, "%2d: just receive\n", cpu);
	}
    }

    pan_mutex_lock(data_lock);
    while (nr_data < total_data) {
	pan_cond_wait(data_done);
    }
    pan_mutex_unlock(data_lock);

    pan_time_get(stop);
    pan_time_sub(stop, start);

    send_report(pan_time_t2d(stop));

    pan_time_clear(start);
    pan_time_clear(stop);

    pan_mutex_lock(sync_lock);
    n_attempts = 0;
    while (nr_reports != total_reports) {
	pan_time_get(t);
	pan_time_add(t, timeout);
#ifdef POLL_ON_WAIT
	if (poll_on_sync) {
	    pan_mutex_unlock(sync_lock);
	    pan_poll();
	    pan_mutex_lock(sync_lock);
	    pan_time_get(stop);
	    if (pan_time_cmp(stop, t) > 0) {
		++n_attempts;
		if (n_attempts >= MAX_RETRIAL) {
		    if (verbose >= 1) {
			printf("%2d: quit abnormally\n", cpu);
		    }
		    break;
		}
	    }
	} else {
	    if (! pan_cond_timedwait(reports_arrived, t)) {
		++n_attempts;
		if (n_attempts >= MAX_RETRIAL) {
		    if (verbose >= 1) {
			printf("%2d: quit abnormally\n", cpu);
		    }
		    break;
		}
	    }
	}
#else
	if (! pan_cond_timedwait(reports_arrived, t)) {
	    ++n_attempts;
	    if (n_attempts >= MAX_RETRIAL) {
		if (verbose >= 1) {
		    printf("%2d: quit abnormally\n", cpu);
		}
		break;
	    }
	}
#endif
    }
    pan_mutex_unlock(sync_lock);

    clear_data_monitor();
    clear_sync_monitor();

    mcast_msg_end(statistics);

    pan_pset_clear(all);

    pan_end();

    if (cpu == reporter) {
#ifdef ANIL_FORMAT
	std_dt = stdev(sq_sum_time, sum_time, total_reports);
	sum_time /= total_reports;
	throughput = nsender * msg_size * nr_multicasts / (1024 * sum_time);
	std_thr = std_dt * throughput / sum_time;
	printf(">>>>>>>>>>>>>>>>>>>>>\n");
	printf("%d senders sent %d reliable multicasts of %d ",
		nsender, nr_multicasts, msg_size);
	printf("data bytes in %f +- %f seconds ( => %f +- %f Kbytes/sec)\n",
		sum_time, std_dt, throughput, std_thr);
	if (ncpu > 1) {
	    printf("(On a mesh, this is a link usage of %f +- %f Kbytes/sec)\n",
		    throughput * (ncpu - 1), std_thr * (ncpu - 1));
	}
	printf("<<<<<<<<<<<<<<<<<<<<<\n");
#else
	sum_time /= total_reports;
	printf("%2d:      total time  : %f s in %d ticks\n",
		cpu, sum_time, nr_multicasts);
	printf("%2d:      per tick    : %f s\n",
		cpu, sum_time / (nr_multicasts * nsender));
#endif
    }

    if (statistics) {
#ifndef NOSTATISTICS
	printf("%2d: Sent data   report Rcvd data   report\n",
		cpu);
	printf("%2d:     %5d %5d      %5d %5d\n",
		cpu, data_sent, report_sent, data_rcvd, report_rcvd);
#endif		/* STATISTICS */
    }

    pan_time_clear(t);
    pan_time_clear(timeout);

    clear_sender_memo();

    return 0;
}
