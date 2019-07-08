#include <assert.h>

#include "pan_sys.h"

#include "pan_trace.h"

#include "global.h"
#include "dispatch.h"
#include "grp.h"
#include "group_tab.h"
#include "header.h"
#include "send.h"

#include "mcast_frag_buf.h"
#include "mcast_snd_buf.h"
#include "mcast_ticket.h"


static int SND_TICKET_SIZE = 8;		/* The number of outstanding group
					 * msgs from this platform. Decrease
					 * of this number throttles the flow.
					 */

static pan_pset_p  all;
static pan_mutex_p grp_snd_lock;


#ifdef GRP_FRAGMENT_DAEMON
#  define MCAST_FROM_UPCALL
#endif

#ifdef GRP_FRAGMENT_DAEMON

static void
snd_late(void *arg)
{
    snd_buf_p      buf = (snd_buf_p)arg;
    grp_hdr_t      h;
    grp_hdr_p      hdr;
    pan_msg_p      msg;
    pan_fragment_p frag;

    trc_new_thread(0, "grp send daemon");
    while (x_snd_buf_get(buf, &h, &msg)) {
	frag = pan_msg_next(msg);
	hdr  = pan_fragment_header(frag);
	*hdr = h;			/* hdr need not be changed */
	pan_comm_multicast_fragment(all, frag);
    }
    pan_thread_exit();
}


static snd_buf_p   snd_buf;
static pan_thread_p snd_daemon;

#endif		/* GRP_FRAGMENT_DAEMON */


static msg_list_p  snd_tickets;
static frag_buf_p  frag_buf;


void
pan_grp_snd_start(void)
{
    all = pan_pset_create();
    pan_pset_fill(all);
    grp_snd_lock   = pan_mutex_create();

				/* Create the ticket list */
    snd_tickets    = msg_list_create(SND_TICKET_SIZE, grp_snd_lock);
				/* Create the fragment assembly buffer */
    frag_buf       = frag_buf_create(pan_grp_n_platforms, SND_TICKET_SIZE);

#ifdef GRP_FRAGMENT_DAEMON
				/* The send buf size must be >= snd_ticket size
				 * to avoid deadlock */
    snd_buf        = snd_buf_create(grp_snd_lock, SND_TICKET_SIZE);
    snd_daemon     = pan_thread_create(snd_late, snd_buf, 0,
				       pan_thread_maxprio(), 0);
#endif		/* GRP_FRAGMENT_DAEMON */
}


void
pan_grp_snd_end(void)
{
#ifdef GRP_FRAGMENT_DAEMON
    snd_poison(snd_buf);
    pan_thread_join(snd_daemon);
    pan_thread_clear(snd_daemon);
    snd_buf_clear(snd_buf);
#endif		/* GRP_FRAGMENT_DAEMON */

    pan_pset_clear(all);
    frag_buf_clear(frag_buf);
    msg_list_clear(snd_tickets);
    pan_mutex_clear(grp_snd_lock);
}



#ifdef MCAST_FROM_UPCALL


/* From this function, just send the first fragment, then return. From the
 * receiving upcall, the other fragments are sent consecutively. */
void
pan_group_send(pan_group_p grp, pan_msg_p msg)
{
    grp_hdr_p      hdr;
    pan_fragment_p frag;

    frag = pan_msg_fragment(msg, pan_grp_nsap);
    hdr  = pan_fragment_header(frag);
    hdr->ticket = msg_list_x_get(snd_tickets, msg);
    hdr->type   = G_SEND;
    hdr->sender = pan_grp_me;
    hdr->gid    = pan_grp_gid(grp);

    pan_comm_multicast_fragment(all, frag);
}


#else		/* MCAST_FROM_UPCALL */


/* From this function, send the fragments in a loop. Await receipt of
 * the previous fragment, since only one coexisting fragment is allowed. */
void
pan_group_send(pan_group_p grp, pan_msg_p msg)
{
    grp_hdr_p      hdr;
    grp_hdr_t      h;
    pan_fragment_p frag;
    boolean        is_last_frag;

    frag = pan_msg_fragment(msg, pan_grp_nsap);

    h.ticket = msg_list_x_get(snd_tickets, msg);
    h.type   = G_SEND;
    h.sender = pan_grp_me;
    h.gid    = pan_grp_gid(grp);

    do {
	is_last_frag = pan_fragment_flags(frag) & PAN_FRAGMENT_LAST;
	hdr  = pan_fragment_header(frag);
	*hdr = h;

	pan_comm_multicast_fragment(all, frag);

	if (is_last_frag) {
	    /* 
	     * Don't touch the last fragment after the multicast;
	     * it may have been delivered!!
	     */
	    return;
	}

	msg_list_x_wait(snd_tickets, h.ticket);

    } while (pan_msg_next(msg));
}


#endif		/* MCAST_FROM_UPCALL */


void
pan_grp_handle_send(grp_hdr_p hdr, pan_fragment_p frag)
{
    pan_group_p     grp = pan_gtab_x_find(hdr->gid);
    pan_msg_p       catch_msg;
    boolean         is_last_frag;
    int             sender;
    short int       ticket;
    unsigned int    frag_flags;
#if (defined MCAST_FROM_UPCALL) && ! (defined GRP_FRAGMENT_DAEMON)
    grp_hdr_t       h;
#endif

    if (! grp) {
	return;					/* discard fragment */
    }

    sender = hdr->sender;	/* copy because assemble may destroy *hdr */
    ticket = hdr->ticket;

    assert(sender >= 0);
    assert(sender < pan_grp_n_platforms);
    assert(ticket >= 0);
    assert(ticket < SND_TICKET_SIZE);

    pan_mutex_lock(grp_snd_lock);

    if (! pan_pset_ismember(grp->members, pan_grp_me)) {
	pan_mutex_unlock(grp_snd_lock);
	return;
    }

    frag_flags   = pan_fragment_flags(frag);
    is_last_frag = ((frag_flags & PAN_FRAGMENT_LAST) != 0);

    if (sender == pan_grp_me) {		/* Get msg from send buf */

	catch_msg = msg_list_locate(snd_tickets, ticket);

#ifdef MCAST_FROM_UPCALL

	if (is_last_frag) {
	    pan_msg_next(catch_msg);	/* ???? Why? ???? */
	    msg_list_put(snd_tickets, ticket);

	} else {
#  ifdef GRP_FRAGMENT_DAEMON
	    snd_buf_put(snd_buf, hdr, catch_msg);
#  else		/* GRP_FRAGMENT_DAEMON */
	    h = *hdr;
	    frag = pan_msg_next(catch_msg);
	    hdr  = pan_fragment_header(frag);
	    *hdr = h;			/* hdr need not be changed */
	    pan_comm_multicast_fragment(all, frag);
#  endif		/* GRP_FRAGMENT_DAEMON */
	}

#else		/* MCAST_FROM_UPCALL */

	if (is_last_frag) {
	    msg_list_put(snd_tickets, ticket);
	} else {
	    msg_list_signal(snd_tickets, ticket);
	}

#endif		/* MCAST_FROM_UPCALL */

    } else if (frag_flags & PAN_FRAGMENT_FIRST) {	/* Start catch */

	catch_msg = pan_msg_create();
	pan_msg_assemble(catch_msg, frag, 0);
	if (! is_last_frag) {
	    frag_buf_store(frag_buf, catch_msg, sender, ticket);
	}

    } else {					/* Get msg from rcve buf */
	catch_msg = frag_buf_locate(frag_buf, sender, ticket);
	pan_msg_assemble(catch_msg, frag, 0);

    }

    if (is_last_frag) {
	if (grp->receiver == NULL) {
	    pan_panic("group receive upcall not installed\n");
	}
	grp->receiver(catch_msg);		/* Do upcall to application */
    }

    pan_mutex_unlock(grp_snd_lock);
}
