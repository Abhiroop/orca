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
    REPORT
} msg_type_t, *msg_type_p;


static char *
msg_type_asc(msg_type_t tp)
{
    switch (tp) {
    case DATA:		return "DATA";
    case REPORT:	return "REPORT";
    }
    return NULL;	/*NOTREACHED*/
}
    

static pan_timer_p send_latency;	/* timer for latency between send
					 * and receive */

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
static int	sync_send = TRUE;	/* sync mcast? */
static int      msg_size = 0;		/* max msg size */


#ifndef NOSTATISTICS

static int      upcalls   = 0;
static int	data_sent = 0;
static int	data_rcvd = 0;
static int	report_sent = 0;
static int	report_rcvd = 0;
static int	mcast_retries = 0;

#define STATINC(stat,n)	do { (stat) += (n); } while (0)
#define GETSTAT(n,stat) do { (*n) = (stat); } while (0)

#else		/* STATISTICS */

#define STATINC(stat,n)
#define GETSTAT(n,stat)

#endif		/* STATISTICS */

static pan_pset_p all;

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


/* Monitor for sync barrier between senders */

static pan_mutex_p	sync_lock;

static int		report_seqno;		/* seq no of current report */
static int	       *report_vector;		/* sync from other senders */
static pan_cond_p       reports_arrived;	/* CV that nr_reports == ncpu */
static int              nr_reports;
static int              total_reports;



typedef struct FRAG_HDR_T {
    msg_type_t type;
    int        sender;
    int        tag;
    int        ticket;
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




/*--- module msg_lst ---------------------------------------------------------*/

typedef struct msg_list_item msg_link_t, *msg_link_p;

struct msg_list_item {
    msg_link_p     next;
    pan_msg_p      msg;
};

typedef struct msg_list msg_list_t, *msg_list_p;

struct msg_list {
    msg_link_p     item;
    msg_link_p     free_list;
    pan_mutex_p    lock;
    pan_cond_p     available;
};


static void
msg_list_init(msg_list_p lst, int size, pan_mutex_p lock)
{
    int i;

    lst->item = pan_calloc(size, sizeof(msg_link_t));
    lst->free_list = &lst->item[0];
    for (i = 0; i < size - 1; i++) {
	lst->item[i].next = &lst->item[i+1];
    }
    lst->item[size - 1].next = NULL;
    lst->lock = lock;
    lst->available = pan_cond_create(lst->lock);
}

static void
msg_list_clear(msg_list_p lst)
{
    pan_cond_clear(lst->available);
    pan_free(lst->item);
}

static short int
msg_list_x_get(msg_list_p lst, pan_msg_p msg)
{
    short int ticket;

    pan_mutex_lock(lst->lock);
    while (lst->free_list == NULL) {
	pan_cond_wait(lst->available);
    }

    ticket = lst->free_list - &lst->item[0];
    assert(lst->free_list->msg == NULL);
    lst->free_list->msg = msg;

    lst->free_list = lst->free_list->next;

    if (lst->free_list != NULL) {
	pan_cond_signal(lst->available);
    }
    pan_mutex_unlock(lst->lock);

    return ticket;
}

static void
msg_list_put(msg_list_p lst, short int ticket)
{
    msg_link_p ticket_slot;

    if (lst->free_list == NULL) {
	pan_cond_signal(lst->available);
    }
    ticket_slot = &lst->item[ticket];
    ticket_slot->next = lst->free_list;
    lst->free_list = ticket_slot;
#ifndef NDEBUG
    ticket_slot->msg = NULL;
#endif
}

static pan_msg_p
msg_list_locate(msg_list_p lst, short int ticket)
{
    return lst->item[ticket].msg;
}


/*--- End of module msg_lst --------------------------------------------------*/


/*--- module frag_buf --------------------------------------------------------*/

typedef struct frag_buf {
    pan_msg_p  *buf;
    int         n_x;
    int         n_y;
} frag_buf_t, *frag_buf_p;

static frag_buf_p
frag_buf_create(int n_senders, short int n_tickets)
{
    frag_buf_p f;

    f = pan_malloc(sizeof(frag_buf_t));
    f->n_x = n_senders;
    f->n_y = n_tickets;
    f->buf = pan_malloc(n_senders * n_tickets * sizeof(pan_msg_p));

    return f;
}

static void
frag_buf_clear(frag_buf_p lst)
{
    pan_free(lst->buf);
    pan_free(lst);
}

static void
frag_buf_store(frag_buf_p lst, pan_msg_p msg, int sender, short int ticket)
{
    assert(sender >= 0);
    assert(sender < lst->n_x);
    assert(ticket >= 0);
    assert(ticket < lst->n_y);
    lst->buf[sender * lst->n_y + ticket] = msg;
}

static pan_msg_p
frag_buf_locate(frag_buf_p lst, int sender, short int ticket)
{
    assert(sender >= 0);
    assert(sender < lst->n_x);
    assert(ticket >= 0);
    assert(ticket < lst->n_y);
    return lst->buf[sender * lst->n_y + ticket];
}


/*--- End of module frag_buf -------------------------------------------------*/


static int SND_TICKET_SIZE = 8;		/* The number of outstanding group
					 * msgs from this platform. Decrease
					 * of this number throttles the flow.
					 */


#ifdef NO_MCAST_FROM_UPCALL


/*--- Module snd_buf ---------------------------------------------------------*/


#define SND_BUF_SIZE	32		/* MUST be a power of 2 */

#define SND_INDEX(n)	(n & (SND_BUF_SIZE - 1))


typedef struct snd_buf {
    frag_hdr_t      hdr[SND_BUF_SIZE];
    pan_msg_p       msg[SND_BUF_SIZE];
    int             next;
    int             last;
    pan_mutex_p     lock;
    pan_cond_p      non_empty;
    pan_cond_p      non_full;
    int             done;
} snd_buf_t, *snd_buf_p;


static void
snd_buf_init(snd_buf_p buf, pan_mutex_p lock)
{
    buf->next      = 0;
    buf->last      = 0;
    buf->lock      = lock;
    buf->non_empty = pan_cond_create(buf->lock);
    buf->non_full  = pan_cond_create(buf->lock);
    buf->done      = FALSE;
}


static void
snd_buf_clear(snd_buf_p buf)
{
    pan_cond_clear(buf->non_empty);
    pan_cond_clear(buf->non_full);
}


static void
snd_buf_put(snd_buf_p buf, frag_hdr_p hdr, pan_msg_p msg)
{
    int idx;
    int     must_wait = FALSE;

    while (buf->next - buf->last == SND_BUF_SIZE) {
	must_wait = TRUE;
	pan_cond_wait(buf->non_full);
    }
    if (must_wait) {
	pan_cond_signal(buf->non_full);
    }

    idx = SND_INDEX(buf->next);
    buf->hdr[idx] = *hdr;
    buf->msg[idx] = msg;
    ++buf->next;

    pan_cond_signal(buf->non_empty);
}


static int
x_snd_buf_get(snd_buf_p buf, frag_hdr_p hdr, pan_msg_p *msg)
{
    int idx;

    pan_mutex_lock(buf->lock);
    while (buf->next == buf->last && ! buf->done) {
	pan_cond_wait(buf->non_empty);
	if (buf->done) {
	    pan_mutex_unlock(buf->lock);
	    return FALSE;
	}
    }

    idx = SND_INDEX(buf->last);
    *hdr = buf->hdr[idx];
    *msg = buf->msg[idx];
#ifndef NDEBUG
    buf->msg[idx] = NULL;
#endif
    ++buf->last;

    pan_mutex_unlock(buf->lock);

    return TRUE;
}


static void
snd_poison(snd_buf_p buf)
{
    pan_mutex_lock(buf->lock);
    buf->done = TRUE;
    pan_cond_signal(buf->non_empty);
    pan_mutex_unlock(buf->lock);
}

/*--- End of module snd_buf --------------------------------------------------*/


static void
snd_late(void *arg)
{
    snd_buf_p      buf = (snd_buf_p)arg;
    frag_hdr_t     h;
    frag_hdr_p     hdr;
    pan_msg_p      msg;
    pan_fragment_p frag;

    while (x_snd_buf_get(buf, &h, &msg)) {
	frag = pan_msg_next(msg);
	hdr  = pan_fragment_header(frag);
	*hdr = h;			/* hdr need not be changed */
	pan_comm_multicast_fragment(all, frag);
    }
    pan_thread_exit();
}


static snd_buf_t   snd_buf;
static pan_thread_p snd_daemon;

#endif		/* NO_MCAST_FROM_UPCALL */



static msg_list_t  snd_tickets;
static msg_list_t  single_tickets;
static frag_buf_p  frag_buf;



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
    hdr = pan_fragment_header(frag);
    hdr->sender     = sender;
    hdr->type       = type;
    hdr->tag        = tag;
    if (pan_fragment_flags(frag) & PAN_FRAGMENT_LAST) {
	hdr->ticket = msg_list_x_get(&single_tickets, msg);
    } else {
	hdr->ticket = msg_list_x_get(&snd_tickets, msg);
    }
    if (verbose >= 40) {
	printf("mcast: fragment flags = %x\n", pan_fragment_flags(frag));
    }
    pan_comm_multicast_fragment(mc_set, frag);
}


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
    reliable_mcast(all, msg, REPORT, cpu, 0);
    STATINC(report_sent,1);
    pan_msg_clear(msg);
}


static void
handle_report(pan_fragment_p frag, frag_hdr_p hdr)
{
    report_msg_p report_body;
    pan_msg_p    catch;

    STATINC(report_rcvd,1);

    catch = pan_msg_create();
    pan_msg_assemble(catch, frag, 0);
    report_body = report_msg_pop(catch);

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

    pan_msg_clear(catch);
}


static void
handle_data(pan_fragment_p frag, frag_hdr_p hdr)
{
    pan_msg_p       catch_msg;
    int             is_first_frag;
    int             is_last_frag;
    int             sender;
    int             send_tag;
    short int       ticket;
    int             i;
    int             size;
    char           *data;
#ifndef NO_MCAST_FROM_UPCALL
    frag_hdr_t      h;
#endif

    STATINC(data_rcvd,1);

    sender   = hdr->sender;	/* copy because assemble may destroy *hdr */
    ticket   = hdr->ticket;
    send_tag = hdr->tag;

    assert(sender >= 0);
    assert(sender < ncpu);
    assert(ticket >= 0);
    assert(ticket < SND_TICKET_SIZE);

    pan_mutex_lock(mcast_lock);

    is_first_frag = (pan_fragment_flags(frag) & PAN_FRAGMENT_FIRST);
    is_last_frag  = (pan_fragment_flags(frag) & PAN_FRAGMENT_LAST);

    if (sender == cpu) {		/* Get msg from send buf */

	if (is_first_frag && is_last_frag) {
	    catch_msg = msg_list_locate(&single_tickets, ticket);
	    pan_msg_next(catch_msg);		/* ???? Why? ???? */
	    msg_list_put(&single_tickets, ticket);

	} else {
	    catch_msg = msg_list_locate(&snd_tickets, ticket);
	    if (is_last_frag) {
		pan_msg_next(catch_msg);	/* ???? Why? ???? */
		msg_list_put(&snd_tickets, ticket);

	    } else {
#ifdef NO_MCAST_FROM_UPCALL
		snd_buf_put(&snd_buf, hdr, catch_msg);
#else		/* NO_MCAST_FROM_UPCALL */
		h = *hdr;
		frag = pan_msg_next(catch_msg);
		hdr  = pan_fragment_header(frag);
		*hdr = h;			/* hdr need not be changed */
		pan_comm_multicast_fragment(all, frag);
#endif		/* NO_MCAST_FROM_UPCALL */
		catch_msg = NULL;			/* Don't deliver */
	    }
	}

    } else if (is_first_frag) {			/* Start catch */
	catch_msg = pan_msg_create();
	pan_msg_assemble(catch_msg, frag, 0);
	if (! is_last_frag) {
	    frag_buf_store(frag_buf, catch_msg, sender, ticket);
	    catch_msg = NULL;			/* Don't deliver */
	}

    } else {					/* Get msg from rcve buf */
	catch_msg = frag_buf_locate(frag_buf, sender, ticket);
	pan_msg_assemble(catch_msg, frag, 0);

	if (! is_last_frag) {
	    catch_msg = NULL;			/* Don't deliver */
	}
    }

    if (catch_msg != NULL) {
	if (check_msg) {
	    tm_pop_int(catch_msg, &size);
	    assert(size == msg_size);
	    data = pan_msg_look(catch_msg, size, alignof(char));
	    switch (fill_msg) {
	    case FIXED_FILL:
		for (i = 0; i < size; i++) {
		    assert(data[i] == 'a' + sender - (ncpu - nsender));
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
	    tm_push_int(catch_msg, size);
	}
	if (sync_send && sender == cpu) {
	    pan_timer_stop(send_latency);
	    assert(arrived == send_tag - 1);
	    arrived = send_tag;
	    pan_cond_signal(mcast_arrived);
	}
	if (++nr_data == total_data) {
	    pan_cond_signal(data_done);
	}

	if (sender != cpu) {
	    pan_msg_clear(catch_msg);
	}
    }

    pan_mutex_unlock(mcast_lock);
}




static void
plw_receive(pan_fragment_p frag)
{
    frag_hdr_p  hdr;

    STATINC(upcalls,1);

    hdr = pan_fragment_header(frag);

    if (verbose >= 30 || ! is_legal_cpu(hdr->sender)) {
	printf("%2d: rcve hdr = (type = %s), (sender = %d)\n",
		cpu, msg_type_asc(hdr->type), hdr->sender);
    }
    assert(is_legal_cpu(hdr->sender));

    if (verbose >= 40) {
	printf("plw_receive: fragment flags = %x\n", pan_fragment_flags(frag));
    }

#ifdef DEBUG
    printf("%2d: received <%d, %d>\n",
	   cpu, hdr->type, hdr->sender);
#endif

    switch (hdr->type) {

    case REPORT:
	handle_report(frag, hdr);
	break;

    case DATA:
	handle_data(frag, hdr);
	break;

    default:
	pan_panic("Impossible switch case %d in plw_receive\n", hdr->type);

    }

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
    tm_push_int(msg, msg_size);

    for (i = 0; i < nr_multicasts; i++) {
	if (verbose >= 25) {
	    printf("%2d: send multicast %d of size %d\n", cpu, i, msg_size);
	}
	if (sync_send) {
	    pan_timer_start(send_latency);
	}
	reliable_mcast(all, msg, DATA, cpu, i);
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

    seqno_vector = malloc(ncpu * sizeof(int));
    for (i = 0; i < ncpu; i++) {
	seqno_vector[i] = -1;
    }

    msg_list_init(&snd_tickets, SND_TICKET_SIZE, mcast_lock);
    msg_list_init(&single_tickets, SND_TICKET_SIZE, mcast_lock);
    frag_buf = frag_buf_create(ncpu, SND_TICKET_SIZE);

#ifdef NO_MCAST_FROM_UPCALL
    snd_buf_init(&snd_buf, mcast_lock);
    snd_daemon = pan_thread_create(snd_late, &snd_buf, 0, pan_thread_maxprio(),
				   0);
#endif		/* NO_MCAST_FROM_UPCALL */
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
#ifdef NO_MCAST_FROM_UPCALL
    snd_poison(&snd_buf);
    pan_thread_join(snd_daemon);
    pan_thread_clear(snd_daemon);
    snd_buf_clear(&snd_buf);
#endif		/* NO_MCAST_FROM_UPCALL */

    free(seqno_vector);
    pan_cond_clear(mcast_arrived);
    pan_cond_clear(data_done);
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
    printf("   -async\t: don't await receipt of previous msg\n");
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
    char       *mcast_stats;
    int         n;

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
	} else if (strcmp(argv[i], "-async") == 0) {
	    sync_send = FALSE;
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

    all = pan_pset_create();
    pan_pset_fill(all);

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

    send_latency = pan_timer_create();

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

    pan_pset_clear(all);

    pan_nsap_clear(global_nsap);

    for (i = 0; i < ncpu; i++) {
	pan_msg_clear(catch_msg[i]);
    }

    frag_buf_clear(frag_buf);
    msg_list_clear(&snd_tickets);
    msg_list_clear(&single_tickets);

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
	printf("%2d: Sent data   report "
	            "Rcvd data   report "
	            "Retry Upcall\n", cpu);
	printf("%2d:     %5d %5d      %5d %5d  %5d  %5d\n",
		cpu, data_sent, report_sent,
		data_rcvd, report_rcvd,
		mcast_retries, upcalls);
	pan_mcast_va_get_params(NULL,
		"PAN_SYS_MCAST_statistics",	&mcast_stats,
		NULL);
	printf("%s", mcast_stats);
	pan_free(mcast_stats);
#endif		/* STATISTICS */

	n = pan_timer_read(send_latency, t);

	if (n >= 0) {
	    printf("%2d: mcast latency is %f s in %d ticks\n",
		    cpu, pan_time_t2d(t), n);
	}
    }

    pan_timer_clear(send_latency);

    pan_time_clear(t);
    pan_time_clear(timeout);

    clear_sender_memo();

    return 0;
}
