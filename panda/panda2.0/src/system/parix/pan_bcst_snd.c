#include <assert.h>
#include <string.h>
#include <sys/list.h>
#include <sys/sem.h>
#include <sys/comm.h>
#include <sys/link.h>
#include <sys/root.h>

#include "pan_sys.h"

#include "pan_system.h"
#include "pan_error.h"

#include "pan_fragment.h"

#include "pan_comm.h"
#include "pan_comm_inf.h"
#include "pan_msg_cntr.h"
#include "pan_bcst_fwd.h"
#include "pan_bcst_snd.h"
#ifdef UNICAST_DEBUG
#include "pan_ucast.h"
#endif /* UNICAST_DEBUG */



#define MULTICAST_MSG_CONSERVED	1


#define MAX_SND_QUOTA    2


static pan_nsap_p       pan_bcast_nsap;	/* send bcast meta's on this nsap */


#ifdef STATISTICS
#  define STATINC(n)	(++(n))
#  define STATDEC(n)	(--(n))
#else
#  define STATINC(n)
#  define STATDEC(n)
#endif


typedef struct BCAST_SND_STATS {
    int pb_direct_send;
    int pb_indirect_send;
    int gsb_send;
} bcast_snd_stats_t, *bcast_snd_stats_p;


static bcast_snd_stats_t stats;


				/* include data into request up to this size */
#define PB_LENGTH_LIMIT  (90 + sizeof(bcast_hdr_t) + sizeof(int))

static int  near_to_sequencer;


/* Send fragment. The system layer header is pushed, the fragment is enqueued
 * for broadcast. It remains accessible after this function returns. */
static void
pan_comm_bcast_snd_frag(pan_fragment_p qp, int control, int seqno, int sender,
		        pan_msg_counter_p x_counter)
{
#ifdef HOME_FRAGMENT_POINTER
    pan_comm_bcast_snd_new(qp, control, seqno, sender, x_counter);
#else		/* HOME_FRAGMENT_POINTER */
    pan_fragment_p cp;

    cp = pan_fragment_create();
    pan_fragment_copy(qp, cp, MULTICAST_MSG_CONSERVED);

    pan_comm_bcast_snd_new(cp, control, seqno, sender, x_counter);
#endif		/* HOME_FRAGMENT_POINTER */
}



static void
req_async_broadcast(pan_fragment_p frag)
{
    mcast_req_t         mcast_req;
    static RR_Message_t reply_mess;
    int                 size;
    bcast_hdr_p         bcast_hdr;

    mcast_req.i.sender = pan_sys_Parix_id;
    mcast_req.tag      = REQUEST_TAG;

    Wait(&pan_comm_req_lock);		/* one sender at a time speaks to seq */
    pan_msg_counter_hit(&pan_bcast_state.snd_counter);

    bcast_hdr = pan_sys_fragment_comm_hdr_push(frag);
    bcast_hdr->mcast_req = mcast_req;
    size = pan_sys_fragment_nsap_push(frag);

    if (pan_sys_link_connected) {

	if (near_to_sequencer || size >= PB_LENGTH_LIMIT) {
					/* Send an express message */
	    if (SendLink(pan_bcast_req_link, &mcast_req,
			 sizeof(mcast_req_t)) != sizeof(mcast_req_t)) {
		pan_sys_printf("SendLink-Error\n");
	    }
	} else {
					/* Send the whole fragment */
	    if (SendLink(pan_bcast_req_link, frag->data, size) != size) {
		pan_sys_printf("SendLink-Error\n");
	    }
	}

					/* Await feedback from the sequencer */
	if (RecvLink(pan_bcast_req_link, &mcast_req,
		     sizeof(mcast_req_t)) != sizeof(mcast_req_t)) {
	    pan_sys_printf("RecvLink-Error\n");
	}

    } else {

	if (near_to_sequencer || size >= PB_LENGTH_LIMIT) {
					/* Send an express message */
	    mcast_req.tag = SIMPLE_REQUEST_TAG;

					/* and await feedback */
	    if (ExchangeMessage(pan_sys_sequencer, REQ_ORMU, REQ_TYPE,
				mcast_req.tag, -1, &mcast_req,
				sizeof(mcast_req.tag), &reply_mess) !=
			    REPLY_CODE) {
		pan_sys_printf("ExchangeMessage-Error\n");
	    }
	} else {
					/* Send the whole fragment
					 * and await feedback */
	    if (ExchangeMessage(pan_sys_sequencer, REQ_ORMU, REQ_TYPE,
				mcast_req.tag, -1, frag->data, size,
				&reply_mess) != REPLY_CODE) {
		pan_sys_printf("ExchangeMessage-Error\n");
	    }
	}
	mcast_req = *(mcast_req_p)reply_mess.Body;

	assert(mcast_req.tag != PB_REPLY_TAG);
    }

#ifdef DETDEBUG
    pan_sys_printf("request got (no. %d, rep: %d)\n",
		mcast_req.i.seqno, mcast_req.tag);
#endif

    if (mcast_req.tag == PB_REPLY_TAG) {	/* send data to sequencer after
						 * all */
	assert(near_to_sequencer || size >= PB_LENGTH_LIMIT);

	if (SendLink(pan_bcast_data_link, frag->data, size) != size) {
	    pan_sys_printf("SendLink-Error\n");
	}
	STATINC(stats.pb_indirect_send);

#ifdef SINGLE_BCAST_SOURCE
	Signal(&pan_comm_req_lock);
	return;
#endif

    } else if (mcast_req.tag == PB_DONE_TAG) {	/* included data sent as pb */
	assert(size < PB_LENGTH_LIMIT);
	STATINC(stats.pb_direct_send);

#ifdef SINGLE_BCAST_SOURCE
	Signal(&pan_comm_req_lock);
	return;
#endif

    } else {
	assert(mcast_req.tag == REPLY_TAG);	/* gsb: sequencer did not
						 * broadcast */
	STATINC(stats.gsb_send);
    }
    pan_comm_bcast_snd_frag(frag, DATA_MESSAGE, mcast_req.i.seqno,
			    pan_sys_Parix_id, &pan_bcast_state.snd_counter);

    Signal(&pan_comm_req_lock);		/* call pan_comm_bcast_enqueue() in
					 * order of seqno */
}



/*ARGSUSED*/
void
pan_comm_multicast_fragment(pan_pset_p pset, pan_fragment_p frag)
{
#ifdef UNICAST_DEBUG
    if (pan_thread_self() == pan_ucast_rcve_thread)
	pan_sys_printf("multicast from within upcall\n");
#endif

    req_async_broadcast(frag);
}



/*ARGSUSED*/
void
pan_comm_multicast_small(pan_pset_p pset, pan_nsap_p nsap, void *data)
{
    pan_fragment_p frag;

#ifdef UNICAST_DEBUG
    if (pan_thread_self() == pan_ucast_rcve_thread)
	pan_sys_printf("multicast from within upcall\n");
#endif

    frag = pan_comm_small2frag(data, nsap);

    req_async_broadcast(frag);
}



			/* Use this to broadcast meta messages if you are the
			 * sequencer, and already know
			 * seqno and control flags */
void
pan_comm_bcast_snd_small(void *data, int control, int seqno, int sender,
		         pan_msg_counter_p x_counter)
{
    pan_fragment_p frag;
    int size;

    frag = pan_comm_small2frag(data, pan_bcast_nsap);

    pan_sys_fragment_comm_hdr_push(frag);
    size = pan_sys_fragment_nsap_push(frag);
    pan_comm_bcast_snd_new(frag, control, seqno, sender, x_counter);
}



void
pan_comm_send_control_msg(sys_msg_tag_t event)
{
    mcast_req_t mcast_req;

    mcast_req.i.sender = pan_sys_Parix_id;
    mcast_req.tag      = event;

    Wait(&pan_comm_req_lock);

    if (pan_sys_link_connected) {
	if (SendLink(pan_bcast_req_link, &mcast_req,
			sizeof(mcast_req_t)) != sizeof(mcast_req_t)) {
	    pan_sys_printf("SendLink-Error\n");
	}
    } else {
	if (PutMessage(pan_sys_sequencer, REQ_ORMU, REQ_TYPE, mcast_req.tag,
			-1, &mcast_req, 0) != 0) {
	    pan_sys_printf("PutMessage-Error\n");
	}
    }

    Signal(&pan_comm_req_lock);
}



static void
init_stats(void)
{
    stats.pb_direct_send   = 0;
    stats.pb_indirect_send = 0;
    stats.gsb_send         = 0;
}


void
pan_comm_bcast_snd_info(void)
{
    pan_comm_info_set(SEND_PB_DIRECT_SLOT,   stats.pb_direct_send);
    pan_comm_info_set(SEND_PB_INDIRECT_SLOT, stats.pb_indirect_send);
    pan_comm_info_set(SEND_GSB_SLOT,         stats.gsb_send);
}


void
pan_comm_bcast_snd_start(void)
{
    pan_bcast_nsap = pan_nsap_create();
    pan_nsap_small(pan_bcast_nsap, NULL, 0,
		   PAN_NSAP_UNICAST | PAN_NSAP_MULTICAST);

    init_stats();

    if (abs((pan_sys_sequencer % pan_sys_DimX) - pan_sys_x) +
	    abs((pan_sys_sequencer / pan_sys_DimX) - pan_sys_y)
		> (pan_sys_DimX + pan_sys_DimY) / 8)
	near_to_sequencer = 0;
    else
	near_to_sequencer = 1;

    if (pan_sys_protocol & PROTO_pure_GSB) {	/* pure GSB */
	near_to_sequencer = 1;	/* prevent PB */
    }

    pan_msg_counter_init(&pan_bcast_state.snd_counter, MAX_SND_QUOTA, "SEND");
    pan_comm_info_register_counter(SEND_CNT, &pan_bcast_state.snd_counter);
}


void
pan_comm_bcast_snd_end(void)
{
     pan_msg_counter_clear(&pan_bcast_state.snd_counter);

     pan_nsap_clear(pan_bcast_nsap);
}
