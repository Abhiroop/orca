#include <assert.h>
#include <string.h>
#include <sys/list.h>
#include <sys/sem.h>
#include <sys/comm.h>
#include <sys/root.h>
#include <sys/sys_rpc.h>
#include <sys/select.h>

#include "pan_sys.h"

#include "pan_trace.h"

#include "pan_global.h"
#include "pan_system.h"
#include "pan_error.h"
#include "pan_malloc.h"

#include "pan_comm.h"
#include "pan_fragment.h"
#include "pan_comm_inf.h"
#include "pan_deadlock.h"

#include "pan_parix.h"


#ifdef PARIX_PowerPC
#  define UCAST_LINKS
#endif

#ifdef UCAST_NODES
#  undef UCAST_LINKS
#endif

#if (defined UCAST_THREAD_PER_LINK) && (defined UCAST_LINKS)
#  define UCAST_UPCALL_THREAD
#endif



#ifdef STATISTICS
#  define STATINC(n)	(++(n))
#  define STATDEC(n)	(--(n))
#else
#  define STATINC(n)
#  define STATDEC(n)
#endif


static pan_nsap_p        pan_ucast_nsap; /* Send ucast meta's on this nsap */

static int               max_ucast_quota;

static pan_msg_counter_t ucast_counter;	/* ucast-upcalls */

#ifdef UCAST_THREAD_PER_LINK
static pan_thread_p     *pan_ucast_rcve_thread;
static pan_key_p         pan_ucast_key;
#else
pan_thread_p             pan_ucast_rcve_thread;
#endif

#ifndef UCAST_LINKS
static Semaphore_t	*ucast_send_lock;
#endif


#ifdef UCAST_UPCALL_THREAD

static pan_thread_p	 ucast_upcall_thread;

static pan_fragment_p   *ucast_rcve_buffer;	/* [max_ucast_quota] */
static int               ucast_rcve_buffer_next;
static int               ucast_rcve_buffer_last;

static Semaphore_t	 ucast_rcve_free_slots;
static Semaphore_t	 ucast_rcve_full_slots;

#endif		/* UCAST_UPCALL_THREAD */


typedef struct UCAST_STATISTICS {
    int  frag_sent;
    int  frag_sent_queued;
    int  small_sent;
    int  small_sent_queued;
    int  frag_received;
    int  small_received;
    int  slots_ahead;
} ucast_stats_t, *ucast_stats_p;


static ucast_stats_t stats;


#ifdef UCAST_THREAD_PER_LINK
#  define pan_ucast_is_rcve_thread() \
			((int)pan_key_getspecific(pan_ucast_key) == 1)
#else		/* UCAST_THREAD_PER_LINK */
#  define pan_ucast_is_rcve_thread() \
			(pan_thread_self() == pan_ucast_rcve_thread)
#endif		/* UCAST_THREAD_PER_LINK */


static pan_thread_p	 ucast_send_thread;


static int		 max_ucast_buffer;	/* MUST be a power of 2 */
#define QW_MOD(i)	 ((i) & (max_ucast_buffer - 1))


static pan_fragment_p   *ucast_send_buffer;	/* [max_ucast_buffer] */
static int               ucast_send_buffer_next;
static int               ucast_send_buffer_last;

static Semaphore_t	 ucast_send_free_slots;
static Semaphore_t	 ucast_send_full_slots;

#if defined UCAST_THREAD_PER_LINK || defined UCAST_SEND_BY_THREAD
static Semaphore_t	 ucast_send_buffer_lock;
#endif /* defined UCAST_THREAD_PER_LINK || defined UCAST_SEND_BY_THREAD */


static void
pan_ucast_snd_enq(int dest, pan_fragment_p frag, int control)
{
    ucast_hdr_p msgh;

    msgh = pan_sys_fragment_comm_hdr_push(frag);
    msgh->control = control;
    msgh->dest = dest;

    Wait(&ucast_send_free_slots);

#if defined UCAST_THREAD_PER_LINK || defined UCAST_SEND_BY_THREAD
    Wait(&ucast_send_buffer_lock);
#endif /* defined UCAST_THREAD_PER_LINK || defined UCAST_SEND_BY_THREAD */

    assert(ucast_send_buffer[QW_MOD(ucast_send_buffer_next)] == NULL);
    ucast_send_buffer[QW_MOD(ucast_send_buffer_next)] = frag;
    ucast_send_buffer_next++;
    Signal(&ucast_send_full_slots);

#if defined UCAST_THREAD_PER_LINK || defined UCAST_SEND_BY_THREAD
    Signal(&ucast_send_buffer_lock);
#endif /* defined UCAST_THREAD_PER_LINK || defined UCAST_SEND_BY_THREAD */

}


#ifndef UCAST_LINKS

/* ucast_send_daemon
 * A separate send daemon is necessary for those unicast messages that
 * are sent out by the ucast_rcve_daemon; otherwise, deadlock may occur
 * because of the synchronous send/rcve primitives:
 *  - two neighbours, both of whose ucast_rcve_daemons are doing a send
 *    towards each other;
 *  - both are blocked until the other does a receive call.
 */


/*ARGSUSED*/
static void
ucast_send_daemon(void *arg)
{
    int            mod_pos;
    pan_fragment_p qp;
    ucast_hdr_p    msgh;
    int            size;

    ucast_send_buffer_next = 0;

    for (;;) {
	Wait(&ucast_send_full_slots);

	mod_pos = QW_MOD(ucast_send_buffer_last);
	qp = ucast_send_buffer[mod_pos];
	ucast_send_buffer[mod_pos] = NULL;
	ucast_send_buffer_last++;
	Signal(&ucast_send_free_slots);

	msgh = pan_sys_fragment_comm_hdr_look(qp);

	if (msgh->control & STOP_PROGRAM) {
	    break;
	}

	size = pan_sys_fragment_nsap_push(qp);
	Wait(&ucast_send_lock[msgh->dest]);
/* pan_sys_printf("> SendNode/q size %d to %d\n", size, msgh->dest); */
	if (SendNode(msgh->dest, GET_MESS, qp->data, size) != 0) {
	    pan_panic("daemon cannot unicast from %d to %d\n",
			pan_my_pid(), msgh->dest);
	}
/* pan_sys_printf("< SendNode/q to %d\n", msgh->dest); */
	Signal(&ucast_send_lock[msgh->dest]);

#ifdef UNICAST_DEBUG
	pan_sys_printf("unicast/Node fragment to %d done\n", msgh->dest);
#endif

	pan_fragment_clear(qp);
    }

    pan_fragment_clear(qp);
}



/* ucast_rcve_daemon
 *	- listen with RecvNode for unicast messages
 *	- make UPCALLs
 *	- terminate on receive of a 'stop_program' message
 */



/*ARGSUSED*/
static void
ucast_rcve_daemon(void *arg)
{
    int            len;
    pan_fragment_p qp;
    ucast_hdr_p    msgh;
    pan_nsap_p     nsap;
    int            stop;

    trc_new_thread(0, "ucast receiver");

#ifndef NOVERBOSE
    pan_sys_printf("rcve daemon/Node runs\n");
#endif

    qp = pan_fragment_create();

    for (;;) {
/* pan_sys_printf("> RecvNode\n"); */
	len = RecvNode(-1, GET_MESS, qp->data, PACKET_SIZE);
/* pan_sys_printf("< RecvNode\n"); */

	nsap = pan_sys_fragment_nsap_pop(qp, len);
	if (len != qp->size + TOTAL_HDR_SIZE(qp)) {
	    pan_panic("converter: RecvNode failed, len = %d, size = %d + %d\n",
			len, qp->size, TOTAL_HDR_SIZE(qp));
	}

	if (nsap->type == PAN_NSAP_SMALL) {
	    STATINC(stats.small_received);
	} else {
	    STATINC(stats.frag_received);
	}

	msgh = pan_sys_fragment_comm_hdr_pop(qp);

	stop = (msgh->control & STOP_PROGRAM);

#ifdef UCAST_UPCALL_THREAD
	Wait(&ucast_rcve_free_slots);
	assert(ucast_rcve_buffer[QW_MOD(ucast_rcve_buffer_next)] == NULL);
	ucast_rcve_buffer[QW_MOD(ucast_rcve_buffer_next)] = qp;
	ucast_rcve_buffer_next++;
	Signal(&ucast_rcve_full_slots);
	if (stop) {
	    break;
	}

	qp = pan_fragment_create();
#else
	if (stop) {
	    pan_fragment_clear(qp);
	    break;
	}

	detect_enter(ucast_upcall_);
	MAKE_UPCALL(qp);
	detect_exit(ucast_upcall_);
#endif
    }
}


void
pan_comm_unicast_fragment(int pid, pan_fragment_p messi)
{
    int         dest;
    int         size;
    ucast_hdr_p msgh;
    pan_fragment_p copy;

    dest = pan_sys_proc_mapping(pid);
    assert(dest >= 0 && dest < pan_sys_total_platforms);

#ifdef UNICAST_DEBUG
    if (dest == pan_sys_Parix_id)
	pan_sys_printf("unicast/Node to own node\n");
    if (pan_ucast_is_rcve_thread())
	pan_sys_printf("unicast/Node fragment to %d from within upcall\n", pid);
    else
	pan_sys_printf("unicast/Node fragment to %d\n", pid);
#endif

    if (
#ifdef UCAST_SEND_BY_THREAD
	TRUE ||
#endif		/* UCAST_SEND_BY_THREAD */
	pan_ucast_is_rcve_thread()) {
	copy = pan_fragment_create();
	pan_fragment_copy(messi, copy, 1);

	pan_ucast_snd_enq(dest, copy, UNICAST);

	STATINC(stats.frag_sent_queued);
    } else {

	msgh = pan_sys_fragment_comm_hdr_push(messi);
	msgh->control = UNICAST;

	size = pan_sys_fragment_nsap_push(messi);
	Wait(&ucast_send_lock[dest]);
/* pan_sys_printf("> SendNode/frag size %d to %d\n", size, dest); */
	if (SendNode(dest, GET_MESS, messi->data, size) != 0) {
	    pan_panic("cannot unicast fragment from %d to %d\n",
			pan_my_pid(), dest);
	}
/* pan_sys_printf("< SendNode/frag to %d\n", dest); */
	Signal(&ucast_send_lock[dest]);

#ifdef UNICAST_DEBUG
	pan_sys_printf("unicast/Node fragment to %d done\n", pid);
#endif

	STATINC(stats.frag_sent);
    }
}


static void
pan_ucast_send_small(int dest, pan_nsap_p nsap, int cntrl, void *data)
{
    ucast_small_msg_t msg;
    void       *p;
    int         size;
    int         unused;
    pan_fragment_p frag;

    assert(aligned(nsap->data_len, UNIVERSAL_ALIGNMENT));

    if (
#ifdef UCAST_SEND_BY_THREAD
	!(cntrl & STOP_PROGRAM) ||
#endif		/* UCAST_SEND_BY_THREAD */
	pan_ucast_is_rcve_thread()) {
	frag = pan_comm_small2frag(data, nsap);

	pan_ucast_snd_enq(dest, frag, cntrl);

	STATINC(stats.small_sent_queued);
    } else {

	msg.nsap_id     = pan_sys_nsap_index(nsap);
	msg.hdr.control = cntrl;

	unused = MAX_SMALL_SIZE - nsap->data_len;
	p = ((char*)&msg.data) + unused;
	memcpy(p, data, nsap->data_len);
	size = sizeof(ucast_small_msg_t) - unused;

	Wait(&ucast_send_lock[dest]);
/* pan_sys_printf("> SendNode/small size %d to %d\n", size, dest); */
	if (SendNode(dest, GET_MESS, p, size) != 0) {
	    pan_panic("cannot unicast_small from %d to %d\n",
			pan_my_pid(), dest);
	}
/* pan_sys_printf("< SendNode/small to %d\n", dest); */
	Signal(&ucast_send_lock[dest]);
	STATINC(stats.small_sent);
    }
}


#else		/* UCAST_LINKS */


static LinkCB_t   **ucast_link;
static LinkCB_t    *ucast_home_link[2];
static Semaphore_t *ucast_link_lock;

/* ucast_send_daemon
 * A separate send daemon is necessary for those unicast messages that
 * are sent out by the ucast_rcve_daemon; otherwise, deadlock may occur
 * because of the synchronous send/rcve primitives:
 *  - two neighbours, both of whose ucast_rcve_daemons are doing a send
 *    towards each other;
 *  - both are blocked until the other does a receive call.
 */


/*ARGSUSED*/
static void
ucast_send_daemon(void *arg)
{
    int            mod_pos;
    pan_fragment_p qp;
    ucast_hdr_p    msgh;
    int            size;

    ucast_send_buffer_next = 0;

    for (;;) {
	Wait(&ucast_send_full_slots);

	mod_pos = QW_MOD(ucast_send_buffer_last);
	qp = ucast_send_buffer[mod_pos];
	ucast_send_buffer[mod_pos] = NULL;
	ucast_send_buffer_last++;
	Signal(&ucast_send_free_slots);

	msgh = pan_sys_fragment_comm_hdr_look(qp);

	if (msgh->control & STOP_PROGRAM) {
	    break;
	}

	size = pan_sys_fragment_nsap_push(qp);

	Wait(&ucast_link_lock[msgh->dest]);
	if (msgh->dest == pan_sys_Parix_id) {
	    if (SendLink(ucast_home_link[1], qp->data, size) != size) {
		pan_panic("daemon cannot unicast from %d to %d\n", pan_my_pid(), msgh->dest);
	    }
	} else {
	    if (SendLink(ucast_link[msgh->dest], qp->data, size) != size) {
		pan_panic("daemon cannot unicast from %d to %d\n", pan_my_pid(), msgh->dest);
	    }
	}
	Signal(&ucast_link_lock[msgh->dest]);

#ifdef UNICAST_DEBUG
	pan_sys_printf("unicast/Link fragment by thread to %d done\n",
			msgh->dest);
#endif

	pan_fragment_clear(qp);
    }

    pan_fragment_clear(qp);
}


#ifdef UCAST_THREAD_PER_LINK


static Semaphore_t ucast_rcve_lock;



/* ucast_rcve_daemon
 *	- listen with Select, then RecvLink for unicast messages
 *	- make UPCALLs
 *	- terminate on receive of a 'stop_program' message
 */



/*ARGSUSED*/
static void
ucast_rcve_daemon(void *arg)
{
    int            len;
    pan_fragment_p qp;
    ucast_hdr_p    msgh;
    pan_nsap_p     nsap;
    LinkCB_t      *my_link = (LinkCB_t*)arg;
    int            stop;
    char           name[30];

    sprintf(name, "ucast receiver %d", my_link - &ucast_link[0]);
    trc_new_thread(0, name);

#ifndef NOVERBOSE
    pan_sys_printf("rcve daemon/Link runs\n");
#endif

    pan_key_setspecific(pan_ucast_key, (void*)1);

    do {
	qp = pan_fragment_create();

	len = RecvLink(my_link, qp->data, PACKET_SIZE);

	nsap = pan_sys_fragment_nsap_pop(qp, len);
	if (len != qp->size + TOTAL_HDR_SIZE(qp)) {
	    pan_panic("converter: RecvLink failed, len = %d, size = %d + %d\n",
			len, qp->size, TOTAL_HDR_SIZE(qp));
	}

	if (nsap->type == PAN_NSAP_SMALL) {
	    STATINC(stats.small_received);
	} else {
	    STATINC(stats.frag_received);
	}

	msgh = pan_sys_fragment_comm_hdr_pop(qp);

	stop = (msgh->control & STOP_PROGRAM);

	Wait(&ucast_rcve_free_slots);

	Wait(&ucast_rcve_lock);
	assert(ucast_rcve_buffer[QW_MOD(ucast_rcve_buffer_next)] == NULL);
	ucast_rcve_buffer[QW_MOD(ucast_rcve_buffer_next)] = qp;
	ucast_rcve_buffer_next++;
	Signal(&ucast_rcve_lock);

	Signal(&ucast_rcve_full_slots);

#ifdef STATISTICS
	if (ucast_rcve_free_slots.Count > stats.slots_ahead)
	    stats.slots_ahead = ucast_rcve_free_slots.Count;
#endif

    } while (! stop);
}



#else		/* UCAST_THREAD_PER_LINK */



/* ucast_rcve_daemon
 *	- listen with Select, then RecvLink for unicast messages
 *	- make UPCALLs
 *	- terminate on receive of a 'stop_program' message
 */

/*ARGSUSED*/
static void
ucast_rcve_daemon(void *arg)
{
    int            len;
    pan_fragment_p qp;
    ucast_hdr_p    msgh;
    pan_nsap_p     nsap;
    Option_t      *select_opt;
    int            i;
    int            sender;
    int            stop;

    trc_new_thread(0, "ucast receiver");

#ifndef NOVERBOSE
    pan_sys_printf("ucast_rcve_daemon/Link/Select runs\n");
#endif

    qp = pan_fragment_create();

    select_opt = pan_calloc(pan_sys_total_platforms, sizeof(Option_t));
    for (i = 0; i < pan_sys_total_platforms; i++) {
	select_opt[i] = ReceiveOption(ucast_link[i]);
    }

    for (;;) {
	sender = SelectList(pan_sys_total_platforms, select_opt);

	len = RecvLink(ucast_link[sender], qp->data, PACKET_SIZE);

	nsap = pan_sys_fragment_nsap_pop(qp, len);
	if (len != qp->size + TOTAL_HDR_SIZE(qp)) {
	    pan_panic("converter: RecvLink failed, len = %d, size = %d + %d\n",
			len, qp->size, TOTAL_HDR_SIZE(qp));
	}

	if (nsap->type == PAN_NSAP_SMALL) {
	    STATINC(stats.small_received);
	} else {
	    STATINC(stats.frag_received);
	}

	msgh = pan_sys_fragment_comm_hdr_pop(qp);

	stop = (msgh->control & STOP_PROGRAM);

#ifdef UCAST_UPCALL_THREAD
	Wait(&ucast_rcve_free_slots);
	assert(ucast_rcve_buffer[QW_MOD(ucast_rcve_buffer_next)] == NULL);
	ucast_rcve_buffer[QW_MOD(ucast_rcve_buffer_next)] = qp;
	ucast_rcve_buffer_next++;
	Signal(&ucast_rcve_full_slots);
	if (stop) {
	    break;
	}

	qp = pan_fragment_create();
#else
	if (stop) {
	    pan_fragment_clear(qp);
	    break;
	}

	detect_enter(ucast_upcall_);
	MAKE_UPCALL(qp);
	detect_exit(ucast_upcall_);
#endif
    }
}


#endif		/* UCAST_THREAD_PER_LINK */


void
pan_comm_unicast_fragment(int pid, pan_fragment_p messi)
{
    int         dest;
    int         size;
    ucast_hdr_p msgh;
    pan_fragment_p copy;

    dest = pan_sys_proc_mapping(pid);
    assert(dest >= 0 && dest < pan_sys_total_platforms);

#ifdef UNICAST_DEBUG
    if (dest == pan_sys_Parix_id)
	pan_sys_printf("unicast to own node\n");
    if (pan_ucast_is_rcve_thread())
	pan_sys_printf("unicast/Link fragment to %d from within upcall\n", pid);
    else
	pan_sys_printf("unicast/Link fragment to %d\n", pid);
#endif

    if (
#ifdef UCAST_SEND_BY_THREAD
	TRUE ||
#endif		/* UCAST_SEND_BY_THREAD */
	pan_ucast_is_rcve_thread()) {
	copy = pan_fragment_create();
	pan_fragment_copy(messi, copy, 1);

	pan_ucast_snd_enq(dest, copy, UNICAST);

	STATINC(stats.frag_sent_queued);
    } else {

	msgh = pan_sys_fragment_comm_hdr_push(messi);
	msgh->control = UNICAST;

	size = pan_sys_fragment_nsap_push(messi);

	Wait(&ucast_link_lock[dest]);
	if (dest == pan_sys_Parix_id) {
	    if (SendLink(ucast_home_link[1], messi->data, size) != size) {
		pan_panic("cannot unicast fragment from %d to %d\n", pan_my_pid(), dest);
	    }
	} else {
	    if (SendLink(ucast_link[dest], messi->data, size) != size) {
		pan_panic("cannot unicast fragment %d from %d to %d\n",
			  pan_my_pid(), dest);
	    }
	}
	Signal(&ucast_link_lock[dest]);

#ifdef UNICAST_DEBUG
	pan_sys_printf("unicast/Link fragment to %d done\n", pid);
#endif

	STATINC(stats.frag_sent);
    }
}


static void
pan_ucast_send_small(int dest, pan_nsap_p nsap, int cntrl, void *data)
{
    ucast_small_msg_t msg;
    void       *p;
    int         unused;
    int         size;
    pan_fragment_p frag;

    assert(aligned(nsap->data_len, UNIVERSAL_ALIGNMENT));

    if (
#ifdef UCAST_SEND_BY_THREAD
	!(cntrl & STOP_PROGRAM) ||
#endif		/* UCAST_SEND_BY_THREAD */
	pan_ucast_is_rcve_thread()) {
	frag = pan_comm_small2frag(data, nsap);

	pan_ucast_snd_enq(dest, frag, cntrl);

	STATINC(stats.small_sent_queued);
    } else {

	msg.nsap_id     = pan_sys_nsap_index(nsap);
	msg.hdr.control = cntrl;

	unused = MAX_SMALL_SIZE - nsap->data_len;
	p = ((char*)&msg.data) + unused;
	memcpy(p, data, nsap->data_len);
	size = sizeof(ucast_small_msg_t) - unused;

	Wait(&ucast_link_lock[dest]);
	if (dest == pan_sys_Parix_id) {
	    if (SendLink(ucast_home_link[1], p, size) != size) {
		pan_panic("cannot unicast small from %d to %d\n",
			  pan_my_pid(), dest);
	    }
	} else {
	    if (SendLink(ucast_link[dest], p, size) != size) {
		pan_panic("cannot unicast_small from %d to %d\n",
			  pan_my_pid(), dest);
	    }
	}
	Signal(&ucast_link_lock[dest]);
	STATINC(stats.small_sent);
    }
}




static void
connect_up_link(int hops)
{
    int pe;
    int error;

    pe = pan_sys_Parix_id + hops;
    if (pe >= pan_sys_total_platforms) {
	pe -= pan_sys_total_platforms;
    }
    ucast_link[pe] = MakeLink(pe, UCAST_TOP, &error);
    if (ucast_link[pe] == NULL) {
	pan_panic("MakeLink from Parix pe %d to Parix pe %d failed\n",
		  pan_sys_Parix_id, pe);
    }
}


static void
connect_down_link(int hops)
{
    int pe;
    int error;

    pe = pan_sys_Parix_id - hops;
    if (pe < 0) {
	pe += pan_sys_total_platforms;
    }
    ucast_link[pe] = GetLink(pe, UCAST_TOP, &error);
    if (ucast_link[pe] == NULL) {
	pan_panic("GetLink from Parix pe %d to Parix pe %d failed\n",
		  pan_sys_Parix_id, pe);
    }
}


static void
connect_ucast_links(void)
{
    int hops;
    int error;
    int max_hops;
    int i;

    ucast_link = pan_calloc(pan_sys_total_platforms, sizeof(LinkCB_t *));

    max_hops = (pan_sys_total_platforms + 1) / 2;

    for (hops = 1; hops < max_hops; hops++) {
	if ((pan_sys_Parix_id % (2 * hops)) < hops) {
	    connect_up_link(hops);
	    connect_down_link(hops);
	} else {
	    connect_down_link(hops);
	    connect_up_link(hops);
	}
    }

    if ((pan_sys_total_platforms % 2) == 0) {
	if (pan_sys_Parix_id < hops) {
	    connect_up_link(hops);
	} else {
	    connect_down_link(hops);
	}
    }

    error = LocalLink(ucast_home_link);
    if (error < 0) {
	pan_panic("Cannot create ucast_home_link");
    }
    ucast_link[pan_sys_Parix_id] = ucast_home_link[0];

    ucast_link_lock = pan_calloc(pan_sys_total_platforms, sizeof(Semaphore_t));
    for (i = 0; i < pan_sys_total_platforms; i++) {
	InitSem(&ucast_link_lock[i], 1);
    }
}


static void
break_up_link(int hops)
{
    int pe;
    int error;

    pe = pan_sys_Parix_id + hops;
    if (pe >= pan_sys_total_platforms) {
	pe -= pan_sys_total_platforms;
    }
    error = BreakLink(ucast_link[pe]);
    if (error < 0) {
	pan_panic("BreakLink/up from Parix pe %d to Parix pe %d failed\n",
		  pan_sys_Parix_id, pe);
    }
}


static void
break_down_link(int hops)
{
    int pe;
    int error;

    pe = pan_sys_Parix_id - hops;
    if (pe < 0) {
	pe += pan_sys_total_platforms;
    }
    error = BreakLink(ucast_link[pe]);
    if (error < 0) {
	pan_panic("BreakLink/down from Parix pe %d to Parix pe %d failed\n",
		  pan_sys_Parix_id, pe);
    }
}


static void
break_ucast_links(void)
{
    int hops;
    int error;
    int max_hops;

    max_hops = (pan_sys_total_platforms + 1) / 2;
    for (hops = 1; hops < max_hops; hops++) {
	if ((pan_sys_Parix_id % (2 * hops)) < hops) {
	    break_up_link(hops);
	    break_down_link(hops);
	} else {
	    break_down_link(hops);
	    break_up_link(hops);
	}
    }

    if ((pan_sys_total_platforms % 2) == 0) {
	if (pan_sys_Parix_id < hops) {
	    break_up_link(hops);
	} else {
	    break_down_link(hops);
	}
    }

    pan_free(ucast_link);

    error = BreakLink(ucast_home_link[0]);
    if (error < 0) {
	pan_panic("BreakLink/home[0] failed\n");
    }
    error = BreakLink(ucast_home_link[1]);
    if (error < 0) {
	pan_panic("BreakLink/home[1] failed\n");
    }

    pan_free(ucast_link_lock);
}


#endif		/* UCAST_LINKS */


#ifdef UCAST_UPCALL_THREAD

/* ucast_upcall_daemon
 *	- wait until a ucast_rcve_thread has deposited a fragment in the
 *	  rcve_buffer and Signal'ed ucast_rcve_full_slots
 *	- make UPCALLs
 *	- terminate on receive of a 'stop_program' message
 */

/*ARGSUSED*/
static void
ucast_upcall_daemon(void *arg)
{
    int            mod_pos;
    pan_fragment_p qp;
    ucast_hdr_p    msgh;

    trc_new_thread(0, "ucast upcaller");

    ucast_rcve_buffer_next = 0;

    for (;;) {
	Wait(&ucast_rcve_full_slots);

#ifdef UCAST_THREAD_PER_LINK
	Wait(&ucast_rcve_lock);
#endif
	mod_pos = QW_MOD(ucast_rcve_buffer_last);
	qp = ucast_rcve_buffer[mod_pos];
	ucast_rcve_buffer[mod_pos] = NULL;
	ucast_rcve_buffer_last++;
#ifdef UCAST_THREAD_PER_LINK
	Signal(&ucast_rcve_lock);
#endif

	Signal(&ucast_rcve_free_slots);

	msgh = pan_sys_fragment_comm_hdr_look(qp);

	if (msgh->control & STOP_PROGRAM) {
	    break;
	}

	detect_enter(ucast_upcall_);
	MAKE_UPCALL(qp);
	detect_exit(ucast_upcall_);

	pan_fragment_clear(qp);
    }

    pan_fragment_clear(qp);
}


#endif		/* UCAST_UPCALL_THREAD */


void
pan_comm_unicast_small(int pid, pan_nsap_p nsap, void *data)
{
    int         dest;

    assert(aligned(nsap->data_len, UNIVERSAL_ALIGNMENT));

    dest = pan_sys_proc_mapping(pid);
    assert(dest >= 0 && dest < pan_sys_total_platforms);

#ifdef UNICAST_DEBUG
    if (dest == pan_sys_Parix_id)
	pan_sys_printf("unicast_small to own node\n");
    if (pan_ucast_is_rcve_thread())
	pan_sys_printf("unicast small to %d from within upcall\n", pid);
    else
	pan_sys_printf("unicast small to %d\n", pid);
#endif

    pan_ucast_send_small(dest, nsap, UNICAST, data);

#ifdef UNICAST_DEBUG
    pan_sys_printf("unicast small to %d done\n", pid);
#endif
}



static void
init_stats(void)
{
    stats.frag_sent      = 0;
    stats.small_sent     = 0;
    stats.frag_received  = 0;
    stats.small_received = 0;
    stats.slots_ahead    = 0;
}


void
pan_comm_ucast_start(void)
{
#ifdef UCAST_LINKS
    connect_ucast_links();
#else
    int i;

    ucast_send_lock = pan_calloc(pan_sys_total_platforms, sizeof(Semaphore_t));
    for (i = 0; i < pan_sys_total_platforms; i++) {
	InitSem(&ucast_send_lock[i], 1);
    }
#endif

    max_ucast_buffer = 128;		/* MUST be a power of 2 */
    ucast_send_buffer_next = 0;
    ucast_send_buffer_last = 0;

    ucast_send_buffer = pan_calloc(max_ucast_buffer, sizeof(pan_fragment_p));
    InitSem(&ucast_send_free_slots, max_ucast_buffer);
    InitSem(&ucast_send_full_slots, 0);

    pan_ucast_nsap = pan_nsap_create();
    pan_nsap_small(pan_ucast_nsap, NULL, 0, PAN_NSAP_UNICAST);

    pan_comm_info_register_counter(UNICAST_CNT, &ucast_counter);

    init_stats();
    max_ucast_quota  = MIN(pan_sys_total_platforms, 8);

    pan_msg_counter_init(&ucast_counter, max_ucast_quota, "UNICAST");

#ifdef UCAST_UPCALL_THREAD
    ucast_rcve_buffer_next = 0;
    ucast_rcve_buffer_last = 0;

    ucast_rcve_buffer = pan_calloc(max_ucast_buffer, sizeof(pan_fragment_p));
    InitSem(&ucast_rcve_free_slots, max_ucast_buffer);
    InitSem(&ucast_rcve_full_slots, 0);
    ucast_upcall_thread = pan_thread_create(ucast_upcall_daemon, NULL, 0,
					    SYSTEM_PRIORITY_H, 0);
#endif

#if defined UCAST_THREAD_PER_LINK || defined UCAST_SEND_BY_THREAD
    InitSem(&ucast_send_buffer_lock, 1);
#endif /* defined UCAST_THREAD_PER_LINK || defined UCAST_SEND_BY_THREAD */

    ucast_send_thread = pan_thread_create(ucast_send_daemon, NULL, STACK,
					  SYSTEM_PRIORITY_H, 0);

#ifdef UCAST_THREAD_PER_LINK
    InitSem(&ucast_rcve_lock, 1);
    pan_ucast_key = pan_key_create();
    pan_ucast_rcve_thread = pan_calloc(pan_sys_total_platforms,
				       sizeof(pan_thread_p));
    for (i = 0; i < pan_sys_total_platforms; i++) {
	pan_ucast_rcve_thread[i] = pan_thread_create(ucast_rcve_daemon,
						     ucast_link[i], STACK,
						     SYSTEM_PRIORITY_H, 0);
    }

#else		/* UCAST_THREAD_PER_LINK */
    pan_ucast_rcve_thread = pan_thread_create(ucast_rcve_daemon, NULL,
					      0, SYSTEM_PRIORITY_H, 0);
#endif		/* UCAST_THREAD_PER_LINK */

}


void
pan_comm_ucast_end(void)
{
    pan_fragment_p frag;

#ifdef UCAST_THREAD_PER_LINK
    int            i;

				/* Send stop msg over ucast control nsap */
    for (i = 0; i < pan_sys_total_platforms; i++) {
	pan_ucast_send_small(i, pan_ucast_nsap, STOP_PROGRAM, NULL);
    }
    for (i = 0; i < pan_sys_total_platforms; i++) {
	pan_thread_join(pan_ucast_rcve_thread[i]);
    }
    pan_free(pan_ucast_rcve_thread);
#else		/* UCAST_THREAD_PER_LINK */
				/* Send stop msg over ucast control nsap */
    pan_ucast_send_small(pan_sys_Parix_id, pan_ucast_nsap, STOP_PROGRAM, NULL);
    pan_thread_join(pan_ucast_rcve_thread);
    pan_thread_clear(pan_ucast_rcve_thread);
#endif		/* UCAST_THREAD_PER_LINK */

				/* Enq stop msg in ucast send q */
    frag = pan_comm_small2frag(NULL, pan_ucast_nsap);
    pan_ucast_snd_enq(pan_sys_Parix_id, frag, STOP_PROGRAM);

    pan_thread_join(ucast_send_thread);
    pan_thread_clear(ucast_send_thread);
    pan_free(ucast_send_buffer);

#ifdef UCAST_UPCALL_THREAD
    pan_thread_join(ucast_upcall_thread);
    pan_thread_clear(ucast_upcall_thread);
    pan_free(ucast_rcve_buffer);
#endif

    pan_nsap_clear(pan_ucast_nsap);

#ifdef UCAST_LINKS
    break_ucast_links();
#else
    pan_free(ucast_send_lock);
#endif
}


void
pan_comm_ucast_info(void)
{
    pan_comm_info_set(UNICAST_SF_SLOT, stats.frag_sent);
    pan_comm_info_set(UNICAST_SS_SLOT, stats.small_sent);
    pan_comm_info_set(UNICAST_QF_SLOT, stats.frag_sent_queued);
    pan_comm_info_set(UNICAST_QS_SLOT, stats.small_sent_queued);
    pan_comm_info_set(UNICAST_RF_SLOT, stats.frag_received);
    pan_comm_info_set(UNICAST_RS_SLOT, stats.small_received);
    pan_comm_info_set(UNICAST_A_SLOT,  stats.slots_ahead);
}
