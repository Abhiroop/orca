#include "pan_bg.h"
#include "pan_bg_rec.h"
#include "pan_bg_group.h"
#include "pan_bg_send.h"
#include "pan_bg_global.h"
#include "pan_bg_history.h"
#include "pan_bg_ack.h"
#include "pan_bg_bb_list.h"
#include "pan_bg_order_list.h"
#include "pan_bg_hist_list.h"
#include "pan_bg_index.h"
#include "pan_bg_error.h"

/*
 * The receive map contains entries that can be used to assemble ordered
 * fragments. Since all entries arrive in order, all platforms will take
 * the same decision where to assemble the first fragment, so this number
 * can be used to address the other fragments. Each group message
 * contains an index in the send_map, to specifiy where the local sender
 * entry is. If the message is received on the local platform, this entry
 * is signalled, and the receive map entry is passed.
 *
 * When a message is completely received, an upcall is made to the
 * registered upcall_handler.
 *
 * There is a small problem when more messages have to be assembled than
 * there are entries in this table. This can be fixed by using a
 * wait_list (since the fragments are already ordered, the wait_list
 * operations will be consistent), but for the moment we panic on this
 * situation.
 */


typedef struct{
    int          next;
    pan_msg_p    msg;
}entry_t, *entry_p;

typedef struct{
    int index;
    int pid;
    int seqno;
}seqno_hdr_t, *seqno_hdr_p;


/* seqno message (sequencer only) */
static seqno_hdr_t   seqno_header_data;
static seqno_hdr_p   seqno_header = &seqno_header_data;
static pan_nsap_p    seqno_nsap; /* sequence number nsap */


static entry_p table;
static int     table_size;
static int     free_list;
static void  (*upcall_handler)(pan_msg_p msg);


static void seqno_handler(void *data);

void
rec_start(void)
{
    int i;

    table_size = pan_nr_platforms() * 2;
    table = pan_malloc(sizeof(entry_t) * table_size);
    assert(table);

    free_list = -1;
    for(i = table_size - 1; i >= 0; i--){
	table[i].next = free_list;
	table[i].msg  = NULL;
	free_list = i;
    }

    if (pan_my_pid() == 0){
	/* I'm the sequencer */
	
	/* fill in common header fields */
	seqno_header->seqno = -34;
    }

    seqno_nsap = pan_nsap_create();
    pan_nsap_small(seqno_nsap, seqno_handler, sizeof(seqno_hdr_t),
		   PAN_NSAP_UNICAST | PAN_NSAP_MULTICAST);
}

void
rec_end(void)
{
    pan_nsap_clear(seqno_nsap);

    pan_free(table);

    free_list = -1;
}

/*
 * find_entry:
 *                 Finds an empty entry in the map.
 */

static int
find_entry(void)
{
    int entry;

    assert(free_list != -1);
    
    entry = free_list;
    free_list = table[entry].next;
    table[entry].msg = pan_msg_create();

    return entry;
}


/*
 * rec_handle:
 *                 Handles a fragment received from the network. The
 *                 fragments must arrive in a total order. If the
 *                 fragment was sent on this platform, the corresponding
 *                 send map entry is signalled.
 */

void
rec_handle(pan_fragment_p fragment, pan_bg_hdr_p header, int flags)
{
    int pid = header->pid;
    int src = header->src;
    pan_msg_p msg;
    int entry;

    if (header->dest == DEST_ANONYMOUS){
	/* first fragment of a message; find empty entry in map */
	entry = find_entry();
    }else{
	/* not the first part, dest is entry */
	entry = header->dest;
    }

    /* assemble fragment to message */
    pan_msg_assemble(table[entry].msg, fragment, 0);
    pan_fragment_clear(fragment);
    header = NULL;

    /* signal the send entry */
    if (pid == pan_my_pid()){
	send_signal(src, entry);
    }

    if (flags & PAN_FRAGMENT_LAST){
	assert(upcall_handler);

	msg = table[entry].msg;

	/*
         * Make the entry free again.
         */
	table[entry].msg  = NULL;
	table[entry].next = free_list;
	free_list = entry;

	/* make an upcall to the handler */
	pan_mutex_unlock(pan_bg_lock);
	upcall_handler(msg);
	pan_mutex_lock(pan_bg_lock);
    }
}

/*
 * pan_bg_register:
 *                 Registers the upcall handler to which complete group
 *                 messages must be delivered.
 */

void
pan_bg_register(void (*func)(pan_msg_p msg))
{
    assert(upcall_handler == NULL);
    upcall_handler = func;
}


/*
 * send_seqno:
 *                 Send a seqno message to all or one specific platform.
 *                 The sequence number must be set in the header argument.
 */

static void
send_seqno(int dest, pan_bg_hdr_p header)
{
    assert(pan_my_pid() == 0);

    /* fill in seqno_header fields */
    seqno_header->pid   = header->pid;
    seqno_header->index = header->index;
    seqno_header->seqno = header->seqno;

    if (dest == -1){
	pan_comm_multicast_small(pan_bg_all, seqno_nsap, seqno_header);
    }else{
	pan_comm_unicast_small(dest, seqno_nsap, seqno_header);
    }
}

/*
 * pan_bg_orderer:
 *                 Delivers group fragments in order to rec_handle. The
 *                 header must contain the correct sequence number.
 */

void
pan_bg_orderer(pan_fragment_p fragment)
{
    pan_bg_hdr_p header = (pan_bg_hdr_p)pan_fragment_header(fragment);
    int flags = pan_fragment_flags(fragment);
    
    /* check if we're too far behind */
    ack_explicit(header->seqno);

    if (header->seqno == pan_bg_rseqno){
	bb_remove(header->pid, header->index);
	
	/* deliver this ordered fragment */
	rec_handle(fragment, header, flags);
	pan_bg_rseqno++;

	/* check if there are more fragments to deliver */
	while(order_get(pan_bg_rseqno, &fragment)){
	    header = pan_fragment_header(fragment);
	    flags  = pan_fragment_flags(fragment);

	    bb_remove(header->pid, header->index);
	    rec_handle(fragment, header, flags);
	    pan_bg_rseqno++;
	}
    }else{
	/* put fragment in order list for later delivery */
	if (!order_add(header->seqno, fragment)) {
	    pan_fragment_clear(fragment);
	}
    }
}

/*
 * pan_bg_rec_fragment:
 *                 Handles a fragment of a message.
 */

void
pan_bg_rec_fragment(pan_fragment_p fragment, pan_bg_hdr_p header)
{
    pan_fragment_p frag;
    pan_bg_hdr_p   hdr;
    int ret;

    if (pan_my_pid() == 0){
	/* check the piggybacked acknowledgement */
	hist_confirm(ack_check(header->pid, header->ackno));
    }

    switch(index_check(header->index, header->pid)){
    case INDEX_OVERRUN:
	pan_bg_warning("index table overrun %ld\n", header->seqno);
	pan_fragment_clear(fragment);
	break;
    case INDEX_DUPLICATE:
	switch(header->seqno){
	case SEQNO_PB:
	    /*
	    * Duplicate PB fragment. The sender didn't receive the
	    * fragment with sequence number on time. Resend it to the
	    * sender.
	    */
	    assert(pan_my_pid() == 0);

	    if (hist_seqno(header->pid, header->index, &frag)){
		pan_comm_unicast_fragment(header->pid, frag);
	    }
	    pan_fragment_clear(fragment);

	    pan_bg_warning("received duplicate PB from %d index %d\n",
		    header->pid, header->index);

	    break;
	case SEQNO_BB:
	    /*
	    * duplicate fragment: the sender didn't receive the
	    * sequence number in time, so it retransmitted the fragment.
	    * Send a point-to-point SEQNO fragment back containing the
	    * correct sequence number.
	    */

	    /* sequencer handles this */
	    if (pan_my_pid() != 0){
		pan_fragment_clear(fragment);
		break;
	    }

	    ret = hist_seqno(header->pid, header->index, &frag);
	    assert(ret);

	    hdr = pan_fragment_header(frag);
	    send_seqno(header->pid, hdr);

	    pan_fragment_clear(fragment);

	    pan_bg_warning("received duplicate BB from %d index %d\n",
		    header->pid, header->index);

	    break;
	default:
	    /*
             * assert(header->seqno != pan_bg_rseqno); 
             * Can happen if a message from the sender is retransmitted
             * while the sequence number message is not received yet.
             */
	    pan_fragment_clear(fragment);
	    if (pan_my_pid() != 0){
		pan_bg_warning("received duplicate from %d index %d seqno %d\n",
		    header->pid, header->index, header->seqno);
	    }
	}
	break;
    case INDEX_OK: /* all handlers are responsible for the fragment */
	switch(header->seqno){
	case SEQNO_PB:
	    /* handle a PB fragment to the sequencer */

	    assert(pan_my_pid() == 0);

	    header->seqno = pan_bg_seqno;
	    if (!hist_add(pan_bg_seqno, fragment)){
		pan_bg_warning("PB fragment %d not accepted. Out of history space\n",
			pan_bg_seqno);
		pan_fragment_clear(fragment);
		return;
	    }

	    /* set flag to note receipt */
	    index_set(header->index, header->pid);

	    pan_bg_seqno++;
	    pan_comm_multicast_fragment(pan_bg_all, fragment);

	    pan_bg_orderer(fragment);

	    break;
	case SEQNO_BB:
	    if (pan_my_pid() == 0){
		/* received BB fragment at sequencer */

		header->seqno = pan_bg_seqno;
		if (!hist_add(pan_bg_seqno, fragment)){
		    pan_bg_warning("BB fragment %d not accepted. "
			    "Out of history space\n", pan_bg_seqno);
		    pan_fragment_clear(fragment);
		    return;
		}

		/* set flag to note receipt */
		index_set(header->index, header->pid);

		pan_bg_seqno++;

		/* Send corresponding sequence number */
		send_seqno(-1, header);

		pan_bg_orderer(fragment);
	    }else{
		/* set flag to note receipt */
		index_set(header->index, header->pid);

		bb_add(fragment);
	    }
	    break;
	default:
	    /* received a fragment with sequence number */

	    /* set flag to note receipt */
	    index_set(header->index, header->pid);

	    pan_bg_orderer(fragment);
	    break;
	}
    }
}


/*
 * seqno_handler:
 *                 Handle a sequence number fragment. Find the
 *                 corresponding fragment in the bb list.
 *
 *                 If it isn't in the bb list, ask the history of the
 *                 sequencer for this fragment (XXX: can't find this part
 *                 in the code).
 */


static void
seqno_handler(void *data)
{
    seqno_hdr_p header = (seqno_hdr_p)data;
    pan_fragment_p frag;
    pan_bg_hdr_p hdr;

    /* global synchronization */
    pan_mutex_lock(pan_bg_lock);

    if (header->seqno >= pan_bg_rseqno){
	/* Sequencer already handled BB fragment */
	if (pan_my_pid() == 0) return;

	/* didn't receive this fragment yet */
	if (bb_find(header->pid, header->index, &frag)){
	    /* found the fragment, fill in sequence number and order it */
	    hdr = (pan_bg_hdr_p)pan_fragment_header(frag);
	    hdr->seqno = header->seqno;
	    pan_bg_orderer(frag);
	}
    }

    pan_mutex_unlock(pan_bg_lock);
}    




