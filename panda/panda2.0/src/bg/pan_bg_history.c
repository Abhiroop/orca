/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_bg.h"
#include "pan_bg_history.h"
#include "pan_bg_alloc.h"
#include "pan_bg_error.h"
#include "pan_bg_global.h"
#include "pan_bg_group.h"
#include "pan_bg_hist_list.h"
#include "pan_bg_ack.h"
#include "pan_bg_index.h"
#include "pan_bg_rec.h"
#include "pan_bg_order_list.h"

/*
 * Provides an interface to the history mechanism of the sequencer. It
 * asks for a history message if it detects that a message with a higher
 * saequence number has already arrived.
 */

typedef struct{
    int   pid;
    int   seqno;
}hist_hdr_t, *hist_hdr_p;
 
/* history request message */
static hist_hdr_t   hist_header_data;
static hist_hdr_p   hist_header = &hist_header_data;

static pan_nsap_p   hist_nsap;

/*
 * history_handler:
 *                 The sequencer received a history request message. Send
 *                 a reply back to the sender containing this message.
 */

static void
history_handler(void *data)
{
    hist_hdr_p header = (hist_hdr_p)data;
    pan_fragment_p frag;
    pan_bg_hdr_p hdr;
    int ret;

    assert(pan_my_pid() == 0);

    /* global synchronization */
    pan_mutex_lock(pan_bg_lock);

    /* check the piggybacked acknowledgement */
    hist_confirm(ack_check(header->pid, header->seqno - 1));

    ret = hist_find(header->seqno, &frag);

    if (ret){
	hdr = pan_fragment_header(frag);
	
	assert(hdr->seqno == header->seqno);
	
	pan_comm_unicast_fragment(header->pid, frag);
	
	pan_bg_warning("Sent history message with seqno %ld to %d\n", header->seqno,
		header->pid);
    }else{
	pan_bg_warning("History message not found: %d %d\n", header->seqno,
		header->pid);
    }

    pan_mutex_unlock(pan_bg_lock);
}    



void
history_start(void)
{
    /* fill in common header fields */
    hist_header->pid  = pan_my_pid();

    hist_nsap = pan_nsap_create();
    pan_nsap_small(hist_nsap, history_handler, sizeof(hist_hdr_t),
		   PAN_NSAP_UNICAST);
}

void
history_end(void)
{
    pan_nsap_clear(hist_nsap);
}


/*
 * pan_bg_history_tick:
 *                 Checks the history request list every 100 ms.
 *                 Retransmits lost requests.
 */

void
pan_bg_history_tick(int fin)
{
    /* No retransmission requests from sequencer */
    if (pan_my_pid() == 0) return;

    if (order_size() > 0) {
	/* Some messages are missing; ask for the oldest one */

	/* fill in sequence number to ask for */
	hist_header->seqno = pan_bg_rseqno;
	
	/* send message to sequencer */
	pan_comm_unicast_small(0, hist_nsap, (char *)hist_header);
	
	pan_bg_warning("Asked history about seqno: %ld\n", pan_bg_rseqno);
    }
}
