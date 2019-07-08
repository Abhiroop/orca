#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "pan_sys.h"

#include "pan_util.h"
extern double stdev(double sumsqr, double sum, int n);

#ifndef FALSE
#  define FALSE	0
#endif
#ifndef TRUE
#  define TRUE	1
#endif


/*
 * mcast: test sending and receiving of multicast msgs.
 */

/* #define printf pan_sys_printf */
extern int pan_sys_printf(const char *, ...);

#define         BIG             1000000000.0	/* Throughput in Kbytes/sec */
#define		MAX_RETRIAL	15	/* maximum number of retries */
#define		TIMEOUT	        1.0	/* sec */
#define		QUIT_TIMEOUT	10.0	/* sec */


/* Message types: */

typedef enum MSG_TYPE {
    DATA,
    ACK,
    REPORT
} msg_type_t, *msg_type_p;


static char *
msg_type_asc(msg_type_t tp)
{
    switch (tp) {
    case DATA:		return "DATA";
    case ACK:		return "ACK";
    case REPORT:	return "REPORT";
    }
    return NULL;	/*NOTREACHED*/
}
    


static double	sum_time;		/* sum of times needed */
static double	sq_sum_time;		/* sum of squares of times needed */
static int	nsender;		/* nr of sending processes */
static int	cpu;			/* my cpu */
static int	ncpu;			/* tot nr of cpus */

static pan_nsap_p global_nsap;		/* Our communication channel */
static pan_msg_p *catch_msg;		/* For each sender a catch */
static int       *next_seqno;		/* seqnos to not double assemble */


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
static int	sync_send = FALSE;	/* sync mcast? */


#ifndef NOSTATISTICS

static int      upcalls   = 0;
static int	data_sent = 0;
static int	data_rcvd = 0;
static int	ack_sent = 0;
static int	ack_rcvd = 0;
static int	report_sent = 0;
static int	report_rcvd = 0;
static int	mcast_retries = 0;

#define STATINC(stat,n)	do { (stat) += (n); } while (0)
#define GETSTAT(n,stat) do { (*n) = (stat); } while (0)

#else		/* STATISTICS */

#define STATINC(stat,n)
#define GETSTAT(n,stat)

#endif		/* STATISTICS */

static pan_pset_p the_world;

static int       *is_sender_memo;
static int        reporter;


/* Monitor for communication between sender and receiver thread. */

static pan_mutex_p	mcast_lock;

static int		snd_seqno;	/* seq no of msg that is being sent */
static int	       *seqno_vector;	/* Acks from other cpus for snd_seqno */
static int		total_data;	/* total # of data msgs */
static int		nr_data;	/* current # of data msgs */
static pan_cond_p	data_done;	/* CV that all data msgs have arrived */
static int		arrived;	/* tag of my sent message */
static pan_cond_p	mcast_arrived;	/* CV that sent msg has arrived */
static int		nr_acked;	/* nr platforms acked */
static pan_cond_p	all_acked;	/* CV that received acks == ncpu */


/* Monitor for sync barrier between senders */

static pan_mutex_p	sync_lock;

static int		report_seqno;		/* seq no of current report */
static int	       *report_vector;		/* sync from other senders */
static pan_cond_p       reports_arrived;	/* CV that nr_reports == ncpu */
static int              nr_reports;
static int              total_reports;



typedef struct FRAG_HDR_T {
    int        frag_seqno;
    int        msg_seqno;
    msg_type_t type;
    int        sender;
    int        tag;
} frag_hdr_t, *frag_hdr_p;



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




static int
is_legal_cpu(int cpu)
{
    return (cpu >= 0 && cpu < ncpu);
}


static int
is_sender(int cpu)
{
    return is_sender_memo[cpu];
}



#ifdef RELIABLE_MULTICAST


static void
reliable_mcast(
       pan_pset_p  mc_set,
       pan_msg_p   msg,
       msg_type_t  type,
       int         sender,
       int         tag
)
{
    pan_fragment_p frag;
    frag_hdr_p  hdr;

    frag = pan_msg_fragment(msg, global_nsap);
    do {
	hdr = pan_fragment_header(frag);
	hdr->sender     = sender;
	hdr->type       = type;
	hdr->tag        = tag;
	if (verbose >= 40) {
	    printf("mcast: fragment flags = %x\n", pan_fragment_flags(frag));
	}
	pan_comm_multicast_fragment(mc_set, frag);
    } while (pan_msg_next(msg) != NULL);
}


#else		/* RELIABLE_MULTICAST */


static void
reliable_mcast(
       pan_pset_p  mc_set,
       pan_msg_p   msg,
       msg_type_t  type,
       int         sender,
       int         tag
)
{
    int         retrial;		/* nr of retransmissions */
    int         seqno;
    pan_time_p  t = pan_time_create();
    pan_time_p  timeout = pan_time_create();
    pan_fragment_p frag;
    frag_hdr_p  hdr;

    pan_time_d2t(timeout, TIMEOUT);

					/* Protect nr_acked */
    pan_mutex_lock(mcast_lock);

    frag = pan_msg_fragment(msg, global_nsap);

    do {

	seqno = snd_seqno;
	hdr = pan_fragment_header(frag);
	hdr->frag_seqno = seqno;
	hdr->sender     = sender;
	hdr->type       = type;
	hdr->tag        = tag;

	retrial = 0;
	do {
				/* Cannot keep lock during multicast, since
				 * some system implementations do an upcall
				 * within _this_ thread to deliver the home
				 * copy; the upcall then also needs mcast_lock.
				 */
	    pan_mutex_unlock(mcast_lock);

	    if (retrial == MAX_RETRIAL) {
		pan_msg_clear(msg);
		pan_panic("msg %d send failed\n", seqno);
	    }
	    ++retrial;

	    if (verbose > 25 && retrial > 1) {
		printf("%2d: resend msg %d, attempt %d\n",
			cpu, snd_seqno, retrial);
	    }
	    if (verbose >= 40) {
		printf("mcast: fragment flags = %x\n", pan_fragment_flags(frag));
	    }
	    pan_comm_multicast_fragment(mc_set, frag);

	    pan_time_get(t);
	    pan_time_add(t, timeout);
	    pan_mutex_lock(mcast_lock);
	    if (nr_acked != ncpu) {
		pan_cond_timedwait(all_acked, t);
	    }
	} while (nr_acked != ncpu);

	nr_acked = 0;
	++snd_seqno;

	STATINC(mcast_retries, retrial - 1);

    } while (pan_msg_next(msg) != NULL);

    pan_mutex_unlock(mcast_lock);

    pan_time_clear(t);
    pan_time_clear(timeout);
}


#endif		/* RELIABLE_MULTICAST */


static void
send_report(double my_time)
{
    pan_msg_p   msg;

    msg = pan_msg_create();
    report_msg_push(msg, my_time, report_seqno);
    ++report_seqno;

    if (verbose >= 25) {
	printf("%2d: send report\n", cpu);
    }
    					/* Reliably mcast our REPORT */
    reliable_mcast(the_world, msg, REPORT, cpu, 0);
    STATINC(report_sent,1);
    pan_msg_clear(msg);
}


#ifndef RELIABLE_MULTICAST

static void
send_ack(int seqno, int sender)
{
    pan_msg_p   ack_msg;
    pan_fragment_p frag;
    frag_hdr_p  hdr;

    if (verbose >= 25) {
	printf("%2d: send ack (seqno = %d) to sender %d\n", cpu, seqno, sender);
    }
    if (sender == cpu) {
	pan_mutex_lock(mcast_lock);
	if (seqno == snd_seqno &&
		seqno_vector[sender] == snd_seqno - 1) {
	    seqno_vector[sender] = snd_seqno;
	    ++nr_acked;
	    if (nr_acked == ncpu) {	/* Rcvd ack from all; wakeup sender */
		pan_cond_signal(all_acked);
	    }
	}
	pan_mutex_unlock(mcast_lock);
    } else {
	ack_msg = pan_msg_create();
	frag = pan_msg_fragment(ack_msg, global_nsap);
	hdr = pan_fragment_header(frag);
	hdr->frag_seqno = seqno;
	hdr->type = ACK;
	hdr->sender = cpu;
	pan_comm_unicast_fragment(sender, frag);
	if (pan_msg_next(ack_msg) != NULL) {
	    pan_panic("Ack msg does not fit in 1 fragment!\n");
	}
	pan_msg_clear(ack_msg);
    }
    STATINC(ack_sent,1);
}

#endif		/* RELIABLE_MULTICAST */


static void
handle_report(pan_fragment_p frag, frag_hdr_p hdr)
{
    report_msg_p report_body;
    pan_msg_p    catch;
#ifndef RELIABLE_MULTICAST
    int          rcve_seqno;
    int          is_ok;
#endif		/* RELIABLE_MULTICAST */

    STATINC(report_rcvd,1);

    catch = pan_msg_create();
    pan_msg_assemble(catch, frag, 0);
    report_body = report_msg_pop(catch);
    pan_mutex_lock(sync_lock);	/* Protect sync_seqno, nr_sync */
#ifndef RELIABLE_MULTICAST
    rcve_seqno = hdr->frag_seqno;
    is_ok = (report_vector[hdr->sender] == report_body->seqno - 1);
    if (is_ok) {
#else		/* RELIABLE_MULTICAST */
    assert(report_vector[hdr->sender] == report_body->seqno - 1);
#endif		/* RELIABLE_MULTICAST */
	report_vector[hdr->sender] = report_body->seqno;

	if (verbose >= 5 && cpu == reporter) {
	    printf("%2d: updating. Get my_time = %lf from %d\n",
		    cpu, report_body->my_time, hdr->sender);
	}
	sum_time += report_body->my_time;
	sq_sum_time += report_body->my_time * report_body->my_time;

	++nr_reports;
	if (nr_reports == total_reports) {
	    pan_cond_signal(reports_arrived);
	}

#ifndef RELIABLE_MULTICAST
    }
    is_ok |= (report_vector[hdr->sender] == report_body->seqno);
#endif		/* RELIABLE_MULTICAST */

    pan_mutex_unlock(sync_lock);

#ifndef RELIABLE_MULTICAST
    /* Send acknowledgement. */
    if (is_ok) {
	send_ack(rcve_seqno, hdr->sender);
    }
#endif		/* RELIABLE_MULTICAST */

    pan_msg_clear(catch);
}


static void
handle_data(pan_fragment_p frag, frag_hdr_p hdr)
{
    int         rcve_seqno;
    int         msg_size;
    char       *data;
    char        c;
    int         i;
    pan_msg_p   catch;
    int         sender;
    int         send_tag;
    int         flags;

    STATINC(data_rcvd,1);
    sender     = hdr->sender;
    send_tag   = hdr->tag;
    rcve_seqno = hdr->frag_seqno;

    assert(is_sender(sender));
#ifndef RELIABLE_MULTICAST
    if (rcve_seqno == next_seqno[sender]) {
#endif		/* RELIABLE_MULTICAST */

	catch = catch_msg[sender];
	flags = pan_fragment_flags(frag);
	pan_msg_assemble(catch, frag, 0);
	++next_seqno[sender];
	if (flags & PAN_FRAGMENT_LAST) {
	    if (check_msg) {
		tm_pop_int(catch, &msg_size);
		data = pan_msg_pop(catch, msg_size, alignof(char));

		switch (fill_msg) {
		case FIXED_FILL:
		    c = 'a' + sender - (ncpu - nsender);
		    for (i = 0; i < msg_size; i++) {
			if (data[i] != c) {
			    pan_panic("Data msg corrupted at byte %d\n", i);
			}
		    }
		    break;
		case VAR_FILL:
		    for (i = 0; i < msg_size; i++) {
			if (data[i] != ' ' + (i & 63)) {
			    pan_panic("Data msg corrupted at byte %d\n", i);
			}
		    }
		    break;
		default:
		    ;
		}
	    }
	    pan_msg_empty(catch);

	    pan_mutex_lock(mcast_lock);
	    if (sync_send) {
		arrived = send_tag;
		pan_cond_signal(mcast_arrived);
	    }
	    if (++nr_data == total_data) {
		pan_cond_signal(data_done);
	    }
	    pan_mutex_unlock(mcast_lock);
	}

#ifndef RELIABLE_MULTICAST
    } /* else, ignore */

    /* Send acknowledgement. */
    send_ack(rcve_seqno, sender);
#endif		/* RELIABLE_MULTICAST */
}


#ifndef RELIABLE_MULTICAST


static void
handle_ack(pan_fragment_p frag, frag_hdr_p hdr)
{
    int         rcve_seqno;
    frag_hdr_p  hdr;
    int         msg_size;
    char       *data;
    char        c;
    int         i;
    pan_msg_p   catch;
    int         sender;
    int         send_tag;
    int         flags;

    STATINC(ack_rcvd,1);

    rcve_seqno = hdr->frag_seqno;

    catch = pan_msg_create();
    pan_msg_assemble(catch, frag, 0);
    pan_mutex_lock(mcast_lock);
    if (rcve_seqno == snd_seqno && seqno_vector[hdr->sender] == snd_seqno - 1) {
	seqno_vector[hdr->sender] = snd_seqno;
	++nr_acked;
	if (nr_acked == ncpu) {	/* Rcvd ack from all; wakeup sender */
	    pan_cond_signal(all_acked);
	}
    }
    pan_mutex_unlock(mcast_lock);
    pan_msg_clear(catch);
}


#endif		/* RELIABLE_MULTICAST */


static void
plw_receive(pan_fragment_p frag)
{
    int         rcve_seqno;
    frag_hdr_p  hdr;

    STATINC(upcalls,1);

    hdr = pan_fragment_header(frag);
    rcve_seqno = hdr->frag_seqno;

    if (verbose >= 30 || ! is_legal_cpu(hdr->sender)) {
	printf("%2d: rcve hdr = (type = %s), (sender = %d)\n",
		cpu, msg_type_asc(hdr->type), hdr->sender);
    }
    assert(is_legal_cpu(hdr->sender));

    if (verbose >= 40) {
	printf("plw_receive: fragment flags = %x\n", pan_fragment_flags(frag));
    }

#ifdef DEBUG
    printf("%2d: received <%d, %d, %d>\n",
	   cpu, rcve_seqno, hdr->type, hdr->sender);
#endif

    switch (hdr->type) {

    case REPORT:
	handle_report(frag, hdr);
	break;

    case DATA:
	handle_data(frag, hdr);
	break;

#ifndef RELIABLE_MULTICAST
    case ACK:
	handle_ack(frag, hdr);
	break;
#endif		/* RELIABLE_MULTICAST */

    default:
	pan_panic("Impossible switch case %d", hdr->type);

    }

}



static void
send_msgs(int msg_size, int nr_multicasts)
{
    char       *data;
    int         i;
#ifndef RELIABLE_MULTICAST
    int         send_retries;
    int         report_retries;
#endif
    pan_msg_p   msg;

    msg = pan_msg_create();
    data = pan_msg_push(msg, msg_size, alignof(char));
    switch (fill_msg) {
    case FIXED_FILL:
	(void)memset(data, 'a' + cpu - (ncpu - nsender),
		     (size_t)msg_size);
	break;
    case VAR_FILL:
	for (i = 0; i < msg_size; i++) {
	    data[i] = ' ' + (i & 63);
	}
	break;
    default:
	;
    }
    tm_push_int(msg, msg_size);

    for (i = 0; i < nr_multicasts; i++) {
	if (verbose >= 25) {
	    printf("%2d: send multicast %d of size %d\n", cpu, i, msg_size);
	}
	reliable_mcast(the_world, msg, DATA, cpu, i);
	if (sync_send) {
	    pan_mutex_lock(mcast_lock);
	    while (arrived < i) {
		pan_cond_wait(mcast_arrived);
	    }
	    pan_mutex_unlock(mcast_lock);
	}
	STATINC(data_sent,1);
    }

    pan_msg_clear(msg);

#ifndef RELIABLE_MULTICAST
    GETSTAT(&send_retries, mcast_retries);

    GETSTAT(&report_retries, mcast_retries);
    report_retries -= send_retries;

    if (verbose >= 10 && send_retries > 0) {
	printf("%2d: send retries: %d\n", cpu, send_retries);
    }
    if (verbose >= 10 && report_retries > 0) {
	printf("%2d: report retries: %d\n", cpu, report_retries);
    }
#endif
}



static void
init_mcast_monitor(int tot_d)
{
    int i;

    mcast_lock    = pan_mutex_create();
    snd_seqno     = 0;		/* start at sequence number zero */
    data_done     = pan_cond_create(mcast_lock);
    nr_data       = 0;
    total_data    = tot_d;
    mcast_arrived = pan_cond_create(mcast_lock);
    arrived       = -1;
    all_acked     = pan_cond_create(mcast_lock);
    nr_acked      = 0;

    seqno_vector = malloc(ncpu * sizeof(int));
    for (i = 0; i < ncpu; i++) {
	seqno_vector[i] = -1;
    }
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
clear_mcast_monitor(void)
{
    free(seqno_vector);
    pan_cond_clear(mcast_arrived);
    pan_cond_clear(data_done);
    pan_cond_clear(all_acked);
    pan_mutex_clear(mcast_lock);
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
    printf("%s [<cpu_id> <ncpus>] <nsenders> <nr_multicasts> <msg_size>\n",
	   progname);
    printf("options:\n");
    printf("   -check\t: check msg contents (meaningless without -fill)\n");
    printf("   -fill none\t\t: do not fill msg with characters\n");
    printf("   -fill fixed\t\t: fill msg with 1 character\n");
    printf("   -fill var\t\t: fill msg with different characters\n");
    printf("   -scatter\t: scatter senders\n");
    printf("   -statistics\t: print msg statistics\n");
    printf("   -sync\t: await receipt of previous msg\n");
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
    int         msg_size = 0;		/* max msg size */
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
    char       *mcast_stats;

    pan_init(&argc, argv);

    cpu = pan_my_pid();
    ncpu = pan_nr_platforms();

    if (cpu == ncpu) {			/* dedicated sequencer case. Abort */
	pan_end();
	return 0;
    }

    pan_time_d2t(timeout, QUIT_TIMEOUT);

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
	} else if (strcmp(argv[i], "-check") == 0) {
	    check_msg = TRUE;
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
	    case 0: nsender       = atoi(argv[i]);
		    break;
	    case 1: nr_multicasts = atoi(argv[i]);
		    break;
	    case 2: msg_size      = atoi(argv[i]);
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

    the_world = pan_pset_create();
    pan_pset_fill(the_world);

    catch_msg = malloc(ncpu * sizeof(pan_msg_p));
    next_seqno = malloc(ncpu * sizeof(int));
    for (i = 0; i < ncpu; i++) {
	catch_msg[i]  = pan_msg_create();
	next_seqno[i] = 0;
    }

    global_nsap = pan_nsap_create();
    pan_nsap_fragment(global_nsap, plw_receive, sizeof(frag_hdr_t),
			PAN_NSAP_UNICAST | PAN_NSAP_MULTICAST);

    init_sender_memo(scatter, nsender);
    init_mcast_monitor(nsender * nr_multicasts);
    init_sync_monitor(ncpu);

    sum_time = 0.0;
    sq_sum_time = 0.0;

    pan_start();

    pan_time_get(start);

    if (is_sender(cpu)) {	/* i am sender */
	if (verbose >= 1) {
	    printf("%2d: is sender\n", cpu);
	}

	send_msgs(msg_size, nr_multicasts);

    } else {
	if (verbose >= 1) {
	    printf("%2d: just receive\n", cpu);
	}
    }

    pan_mutex_lock(mcast_lock);
    while (nr_data < total_data) {
	pan_cond_wait(data_done);
    }
    pan_mutex_unlock(mcast_lock);

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
    pan_mutex_unlock(sync_lock);

    clear_mcast_monitor();
    clear_sync_monitor();

    pan_pset_clear(the_world);

    pan_nsap_clear(global_nsap);

    for (i = 0; i < ncpu; i++) {
	pan_msg_clear(catch_msg[i]);
    }

    pan_end();

    if (cpu == reporter) {
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
    }

    if (statistics) {
#ifndef NOSTATISTICS
	printf("%2d: Sent data   ack  report "
	            "Rcvd data   ack  report "
	            "Retry Upcall\n", cpu);
	printf("%2d:     %5d %5d %5d      %5d %5d %5d  %5d  %5d\n",
		cpu, data_sent, ack_sent, report_sent,
		data_rcvd, ack_rcvd, report_rcvd,
		mcast_retries, upcalls);
	pan_sys_va_get_params(NULL,
		"PAN_SYS_mcast_statistics",	&mcast_stats,
		NULL);
	printf("%s", mcast_stats);
	pan_free(mcast_stats);
#endif		/* STATISTICS */
    }

    pan_time_clear(t);
    pan_time_clear(timeout);

    clear_sender_memo();

    return 0;
}
