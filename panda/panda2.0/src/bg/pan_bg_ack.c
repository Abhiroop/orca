/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_bg.h"
#include "pan_bg_ack.h"
#include "pan_bg_error.h"
#include "pan_bg_global.h"
#include "pan_bg_group.h"
#include "pan_bg_hist_list.h"

#include <stdio.h>

typedef struct{
    int    type;
    int    ackno;
    int    pid;
}ack_hdr_t, *ack_hdr_p;

#define TYPE_ACKNO     1
#define TYPE_EXP_ACK   2

/* explicit acknowledge message */
static ack_hdr_t   ack_data;
static ack_hdr_p   ack = &ack_data;

#define SEND_EXPL_ACKNO 20

/* acknowledgement list for history management */
static seqno_t       *ack_buffer;
static seqno_t        min_ackno;

static pan_nsap_p  ack_nsap;	/* acknowledgement nsap */


/*
 * handle_exp_ackno:
 *                 Handles an explicit acknowledgement as reply on a
 *                 flush message. This differs from a normal TYPE_ACKNO
 *                 message because they are send asynchronous by a receiver.
 */

static void
handle_exp_ackno(ack_hdr_p header)
{
    pan_fragment_p frag;
    pan_bg_hdr_p hdr;

    /* retransmit next fragment, because it is probably lost */
    if (hist_find(header->ackno + 1, &frag)){
	hdr = pan_fragment_header(frag);
	
	assert(hdr->seqno == header->ackno + 1);
	pan_comm_unicast_fragment(header->pid, frag);
	
	pan_bg_warning("Sent history message after exp ackno %ld to %d\n", 
		header->ackno, header->pid);
    }else{
	pan_bg_warning("Received explicit ackno %d, but can't send next fragment\n",
		header->ackno);
    }
}
    



static void
ack_handler(void *data)
{
    ack_hdr_p header = (ack_hdr_p)data;

    assert(pan_my_pid() == 0);

    /* global synchronization */
    pan_mutex_lock(pan_bg_lock);

    /* check explicit acknowledgement */
    hist_confirm(ack_check(header->pid, header->ackno));

    switch(header->type){
    case TYPE_ACKNO:
	break;
    case TYPE_EXP_ACK:
	handle_exp_ackno(header);
	break;
    default:
	pan_bg_error("Unknown type in ack handler\n");
    }

    pan_mutex_unlock(pan_bg_lock);
}




void
ack_start(void)
{
    /* fill in common header fields */
    ack->type = TYPE_ACKNO;
    ack->pid = pan_my_pid();
    /* ack->ackno filled in when send */

    pan_bg_ackno = 0;

    if (pan_my_pid() == 0){
	/*
	 * for each platform, the lowest sequence number not confirmed yet.
	 */
	ack_buffer = pan_calloc(pan_nr_platforms(), sizeof(seqno_t));
	/* minimum of all acknowledgements */
	min_ackno = 0;
    }

    ack_nsap = pan_nsap_create();
    pan_nsap_small(ack_nsap, ack_handler, sizeof(ack_hdr_t), PAN_NSAP_UNICAST);
}

/*
 * send_ackno:
 *                 Sends an explicit acknowledgement to the sequencer.
 */

static void
send_ackno(int exp)
{
    assert(pan_my_pid() != 0);

    pan_bg_ackno = pan_bg_rseqno - 1;
    ack->type = exp ? TYPE_EXP_ACK : TYPE_ACKNO;
    ack->ackno = pan_bg_rseqno - 1;
    pan_comm_unicast_small(0, ack_nsap, (char *)ack);
}




void
ack_end(void)
{
    pan_time_p now, inc;
    pan_cond_p cond;
    int i;

    cond = pan_cond_create(pan_bg_lock);
    inc = pan_time_create();
    now = pan_time_create();

    if (pan_my_pid() != 0){
	/*
         * send last ackno message a couple of times. XXX: changed to one
         * time for Amoeba termination. 
         */
	pan_time_set(inc, 0L, 500000000L);
	for(i = 0; i < 1; i++) {
	    pan_time_get(now);
	    pan_time_add(now, inc);

	    (void)pan_cond_timedwait(cond, now);

	    printf("Send final ackno\n");
	    send_ackno(0);
	}
    }else{
	pan_time_set(inc, 1L, 0L);
	hist_confirm(ack_check(0, pan_bg_seqno - 1));
	while(min_ackno + 1 < pan_bg_seqno){
	    pan_bg_warning("Waiting for final acknowledgements\n");

	    pan_time_get(now);
	    pan_time_add(now, inc);
	    (void)pan_cond_timedwait(cond, now);
	    hist_confirm(ack_check(0, pan_bg_seqno - 1));
	}
	pan_free(ack_buffer);
    }

    pan_time_clear(inc);
    pan_time_clear(now);
    pan_cond_clear(cond);

    pan_nsap_clear(ack_nsap);
}




/*
 * ack_check:
 *                 Check a received acknowledgement and return the lowest
 *                 sequence number not acknowledged yet.
 */

seqno_t
ack_check(int pid, seqno_t ack)
{
    int i;

    assert(pan_my_pid() == 0);

    /* sequencer has seen all messages */
    ack_buffer[0] = pan_bg_seqno - 1;

    /* check the acknowledgement */
    if (ack >= ack_buffer[pid]){
	ack_buffer[pid] = ack + 1;
	if (min_ackno <= ack){
	    /* try to find better minimum */
	    min_ackno = ack + 1;
	    for(i = 1; i < pan_nr_platforms(); i++){
		if (min_ackno > ack_buffer[i]){
		    min_ackno = ack_buffer[i];
		}
	    }
	}
    }

    return min_ackno;
}

/*
 * ack_explicit:
 *                 Checks how far this platform is behind with its
 *                 acknowledgements. Sends an explicit acknowledgement if
 *                 necessary.
 */

void
ack_explicit(seqno_t seqno)
{
    if (pan_my_pid() != 0 && seqno > pan_bg_ackno + SEND_EXPL_ACKNO){
	send_ackno(0);
    }
}


/*
 * ack_flush:
 *                 Handles a flush request message from the sequencer.
 *                 Send acknowledgement if needed.
 */

void
ack_flush(void)
{
    if (pan_my_pid() != 0){
	send_ackno(1);
	pan_bg_warning("Sent flush reply %d\n", pan_bg_rseqno - 1);
    }
}


/*
 * ack_reply:
 *                 Sequencer handles an ack reply message from pid with
 *                 acknowledgement ackno. Returns 1 if the next fragment
 *                 should be retransmitted, else 0.
 */

int
ack_reply(int pid, seqno_t ackno)
{
    if (ackno + 1 >= min_ackno && pan_bg_seqno > ackno + SEND_EXPL_ACKNO){
	return 1;
    }

    return 0;
}

