#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "pan_sys.h"

#include "pan_timer.h"

#include "pan_util.h"

#include "mcast_frag_buf.h"
#include "mcast_snd_buf.h"
#include "mcast_ticket.h"
#include "mcast_hdr.h"
#include "mcast_msg.h"

#ifndef FALSE
#  define FALSE	0
#endif
#ifndef TRUE
#  define TRUE	1
#endif


/*
 * mcast: fragment & send, assemble & receive of multicast msgs.
 */


#ifdef MCAST_DAEMON
#  define MCAST_FROM_UPCALL
#endif



static pan_nsap_p	mcast_nsap;	/* the nsap for msg mcast */

static pan_pset_p	mcast_set;
static void           (*mcast_msg_upcall)(pan_msg_p msg);	/* Up upcall */

static pan_timer_p	self_latency;	/* timer for latency between send
					 * and next send */
static pan_timer_p	arrive_latency;	/* timer for latency between send
					 * and arrival */
static pan_timer_p	frag_latency;	/* timer for latency between fragment
					 * send and arrival */

static int		mcast_me;	/* cache my cpu */
static int		mcast_nr;	/* cache tot nr of cpus */

static int		verbose = 0;
static int    		sync_send = TRUE;

#ifdef MCAST_DAEMON
static int		high_frag = 0;
#endif
static int		mcast_poll_on_sync;



#ifndef NOSTATISTICS

static int      upcalls   = 0;
#define STATINC(stat)	((stat)++)

#else		/* STATISTICS */

#define STATINC(stat)

#endif		/* STATISTICS */


/* Monitor for synchronous send */

static pan_mutex_p	mcast_lock;

static int              my_sent;	/* count # sent */
static int              my_arrived;	/* count # arrivals */
static pan_cond_p	mcast_arrived;	/* CV that sent msg has arrived */
static pan_cond_p	frag_arrived;	/* CV that sent fragment has arrived */



static int SND_TICKET_SIZE = 8;		/* The number of outstanding mcast
					 * msgs from this platform. Decrease
					 * of this number throttles the flow.
					 */

#ifdef MCAST_DAEMON



static void
snd_late(void *arg)
{
    snd_buf_p      buf = (snd_buf_p)arg;
    frag_hdr_t     h;
    frag_hdr_p     hdr;
    pan_msg_p      msg;
    pan_fragment_p frag;

    while (x_snd_buf_get(buf, &h, &msg)) {
	STATINC(high_frag);
	frag = pan_msg_next(msg);
	hdr  = pan_fragment_header(frag);
	*hdr = h;			/* hdr need not be changed */
	pan_timer_start(frag_latency);
	pan_comm_multicast_fragment(mcast_set, frag);
    }
    pan_thread_exit();
}


static snd_buf_p   snd_buf;
static pan_thread_p snd_daemon;

#endif		/* MCAST_DAEMON */



static msg_list_p  snd_tickets;
static frag_buf_p  frag_buf;



static int
is_legal_cpu(int cpu)
{
    return (cpu >= 0 && cpu < mcast_nr);
}


#ifndef MCAST_FROM_UPCALL


void
mcast_msg(pan_msg_p msg)
{
    pan_fragment_p frag;
    frag_hdr_p     hdr;
    int            my_tag = -1;
    frag_hdr_t     h;

    pan_timer_start(self_latency);
    pan_timer_start(arrive_latency);

    if (sync_send) {
	pan_timer_start(self_latency);
	pan_timer_start(arrive_latency);
    }

    frag = pan_msg_fragment(msg, mcast_nsap);

    h.sender = mcast_me;
    h.ticket = msg_list_x_get(snd_tickets, msg);

    do {
	hdr = pan_fragment_header(frag);
	*hdr = h;

	pan_timer_start(frag_latency);
	pan_mutex_lock(mcast_lock);
	my_tag   = my_sent++;
	pan_mutex_unlock(mcast_lock);
	hdr->tag = my_tag;

	pan_comm_multicast_fragment(mcast_set, frag);

	if (! (pan_fragment_flags(frag) & PAN_FRAGMENT_LAST)) {
	    pan_mutex_lock(mcast_lock);
	    while (my_arrived < my_tag) {
			    /* Two possibilities:
			     * 1. Await upcall to transfer higher fragments
			     * 2. Our msg is early: block to await intermediate
			     *    msg from other sender.
			     */
#ifdef POLL_ON_WAIT
		if (mcast_poll_on_sync) {
		    pan_mutex_unlock(mcast_lock);
		    pan_poll();
		    pan_mutex_lock(mcast_lock);
		} else {
		    pan_cond_wait(frag_arrived);
		}
#else
		pan_cond_wait(frag_arrived);
#endif
	    }
	    pan_mutex_unlock(mcast_lock);
	}
    } while (pan_msg_next(msg));

    if (sync_send) {
	pan_timer_stop(self_latency);
    }
}


#else		/* MCAST_FROM_UPCALL */



void
mcast_msg(pan_msg_p msg)
{
    pan_fragment_p frag;
    frag_hdr_p     hdr;
    int            my_tag = -1;

    if (sync_send) {
	pan_timer_start(self_latency);
	pan_timer_start(arrive_latency);
	pan_timer_start(frag_latency);
	pan_mutex_lock(mcast_lock);
	my_tag      = my_sent++;
	pan_mutex_unlock(mcast_lock);
    }

    frag = pan_msg_fragment(msg, mcast_nsap);

    hdr = pan_fragment_header(frag);
    hdr->sender = mcast_me;
    hdr->ticket = msg_list_x_get(snd_tickets, msg);
    hdr->tag    = my_tag;

    pan_comm_multicast_fragment(mcast_set, frag);

    if (sync_send) {
	pan_mutex_lock(mcast_lock);
	while (my_arrived < my_tag) {
			/* Two possibilities:
			 * 1. Await upcall to transfer higher fragments
			 * 2. Our msg is early: block to await intermediate
			 *    msg from other sender.
			 */
#ifdef POLL_ON_WAIT
	    if (mcast_poll_on_sync) {
		pan_mutex_unlock(mcast_lock);
		pan_poll();
		pan_mutex_lock(mcast_lock);
	    } else {
		pan_cond_wait(mcast_arrived);
	    }
#else
	    pan_cond_wait(mcast_arrived);
#endif
	}
	pan_mutex_unlock(mcast_lock);
	pan_timer_stop(self_latency);
    }
}


#endif		/* MCAST_FROM_UPCALL */


static void
rcve_frag(pan_fragment_p frag)
{
    pan_msg_p       catch_msg;
    unsigned int    frag_flags;
    int             is_last_fragment;
    int             tag;
    int             sender;
    short int       ticket;
    frag_hdr_p      hdr;
#if (defined MCAST_FROM_UPCALL) && ! (defined MCAST_DAEMON)
    frag_hdr_t      h;
#endif

    STATINC(upcalls);

    hdr = pan_fragment_header(frag);

    if (verbose >= 30 || ! is_legal_cpu(hdr->sender)) {
	printf("%2d: rcve hdr = (sender = %d) (ticket = %d) (tag = %d)\n",
		mcast_me, hdr->sender, hdr->ticket, hdr->tag);
    }
    assert(is_legal_cpu(hdr->sender));


#ifdef DEBUG
    printf("%2d: received from %d\n", mcast_me, hdr->sender);
#endif

    sender = hdr->sender;	/* copy because assemble may destroy *hdr */
    ticket = hdr->ticket;
    tag    = hdr->tag;

    assert(sender >= 0);
    assert(sender < mcast_nr);
    assert(ticket >= 0);
    assert(ticket < SND_TICKET_SIZE);

    pan_mutex_lock(mcast_lock);

    frag_flags = pan_fragment_flags(frag);
    is_last_fragment = ((frag_flags & PAN_FRAGMENT_LAST) != 0);

    if (sender == mcast_me) {		/* Get msg from send buf */

	pan_timer_stop(frag_latency);

	catch_msg = msg_list_locate(snd_tickets, ticket);

#ifdef MCAST_FROM_UPCALL

	if (is_last_fragment) {
	    pan_msg_next(catch_msg);	/* ???? Why? ???? */
	    msg_list_put(snd_tickets, ticket);

	} else {
#  ifdef MCAST_DAEMON
	    snd_buf_put(snd_buf, hdr, catch_msg);
#  else
	    h = *hdr;
	    frag = pan_msg_next(catch_msg);
	    hdr  = pan_fragment_header(frag);
	    *hdr = h;			/* hdr need not be changed */
	    pan_comm_multicast_fragment(mcast_set, frag);
#  endif
	}

#else		/* MCAST_FROM_UPCALL */

	if (is_last_fragment) {
	    msg_list_put(snd_tickets, ticket);
	}
	if (! is_last_fragment || sync_send) {
	    pan_timer_stop(arrive_latency);
	    assert(my_arrived == tag - 1);
	    my_arrived = tag;
	    pan_cond_signal(frag_arrived);
	}
#endif

    } else if (frag_flags & PAN_FRAGMENT_FIRST) {	/* Start catch */

	catch_msg = pan_msg_create();
	pan_msg_assemble(catch_msg, frag, 0);
	if (! (frag_flags & PAN_FRAGMENT_LAST)) {
	    frag_buf_store(frag_buf, catch_msg, sender, ticket);
	}

    } else {					/* Get msg from rcve buf */

	catch_msg = frag_buf_locate(frag_buf, sender, ticket);
	pan_msg_assemble(catch_msg, frag, 0);

    }

    if (! is_last_fragment) {
	catch_msg = NULL;			/* Don't deliver */
    }

    if (catch_msg != NULL) {

#ifdef MCAST_FROM_UPCALL
	if (sync_send && sender == mcast_me) {
	    pan_timer_stop(arrive_latency);
	    assert(my_arrived == tag - 1);
	    my_arrived = tag;
	    pan_cond_signal(mcast_arrived);
	}
#endif

	if (verbose >= 100) {
	    printf("%2d: do an upcall\n", mcast_me);
	}
	mcast_msg_upcall(catch_msg);

	if (sender != mcast_me) {
	    pan_msg_clear(catch_msg);
	}
    }

    pan_mutex_unlock(mcast_lock);
}



void
mcast_msg_start(pan_pset_p set, void (*upcall)(pan_msg_p), int snc,
		int poll, int v)
{
    mcast_me           = pan_my_pid();
    mcast_nr           = pan_nr_platforms();

    mcast_lock         = pan_mutex_create();
    mcast_arrived      = pan_cond_create(mcast_lock);
    frag_arrived       = pan_cond_create(mcast_lock);
    my_arrived         = -1;

				/* Create the ticket lists */
    snd_tickets        = msg_list_create(SND_TICKET_SIZE, mcast_lock);
				/* Create the fragment assembly buffer */
    frag_buf           = frag_buf_create(mcast_nr, SND_TICKET_SIZE);

#ifdef MCAST_DAEMON
				/* The send buf size must be >= snd_ticket size
				 * to avoid deadlock */
    snd_buf            = snd_buf_create(mcast_lock, SND_TICKET_SIZE);
    snd_daemon         = pan_thread_create(snd_late, snd_buf, 0,
					   pan_thread_maxprio(), 0);
#endif		/* MCAST_DAEMON */

    self_latency       = pan_timer_create();
    arrive_latency     = pan_timer_create();
    frag_latency       = pan_timer_create();

    mcast_set          = pan_pset_create();
    pan_pset_copy(set, mcast_set);

    verbose            = v;
    sync_send          = snc;
    mcast_poll_on_sync = poll;

    mcast_msg_upcall   = upcall;

					/* Register with lower layer */
    mcast_nsap         = pan_nsap_create();
    pan_nsap_fragment(mcast_nsap, rcve_frag, sizeof(frag_hdr_t),
		      PAN_NSAP_UNICAST | PAN_NSAP_MULTICAST);
}


void
mcast_msg_end(int statistics)
{
    int		n;
    char       *mcast_stats;
    pan_time_p  t = pan_time_create();
    pan_time_p  td = pan_time_create();

    pan_nsap_clear(mcast_nsap);
    pan_pset_clear(mcast_set);

#ifdef MCAST_DAEMON
    snd_poison(snd_buf);
    pan_thread_join(snd_daemon);
    pan_thread_clear(snd_daemon);
    snd_buf_clear(snd_buf);
#endif		/* MCAST_DAEMON */

    frag_buf_clear(frag_buf);
    msg_list_clear(snd_tickets);

    if (statistics) {

	n = pan_timer_read(self_latency, t);
	pan_time_copy(td, t);
	pan_time_div(td, n);
	if (n >= 0) {
	    printf("%2d: Latency %-16s %f s; av. %f s; N %d\n",
		    mcast_me, "msg send-send", pan_time_t2d(t),
		    pan_time_t2d(td), n);
	}
	n = pan_timer_read(arrive_latency, t);
	pan_time_copy(td, t);
	pan_time_div(td, n);
	if (n >= 0) {
	    printf("%2d: Latency %-16s %f s; av. %f s; N %d\n",
		    mcast_me, "msg send-arrv", pan_time_t2d(t),
		    pan_time_t2d(td), n);
	}
	n = pan_timer_read(frag_latency, t);
	pan_time_copy(td, t);
	pan_time_div(td, n);
	if (n >= 0) {
	    printf("%2d: Latency %-16s %f s; av. %f s; N %d\n",
		    mcast_me, "fragm send-arrv", pan_time_t2d(t),
		    pan_time_t2d(td), n);
	}

	printf("%2d: %d fragment upcalls\n", mcast_me, upcalls);

	pan_sys_va_get_params(NULL,
		"PAN_SYS_mcast_statistics",	&mcast_stats,
		NULL);
	printf("%s", mcast_stats);
	pan_free(mcast_stats);

#ifdef MCAST_DAEMON
	printf("%2d: %d fragment daemon wakeups\n", mcast_me, high_frag);
#endif
    }

    pan_timer_clear(self_latency);
    pan_timer_clear(arrive_latency);
    pan_timer_clear(frag_latency);

    pan_mutex_clear(mcast_lock);
    pan_cond_clear(mcast_arrived);
    pan_cond_clear(frag_arrived);

    pan_time_clear(t);
}
