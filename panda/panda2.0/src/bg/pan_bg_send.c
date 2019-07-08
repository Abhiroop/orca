#include "pan_bg.h"
#include "pan_bg_send.h"
#include "pan_bg_error.h"
#include "pan_bg_global.h"
#include "pan_bg_group.h"
#include "pan_bg_index.h"
#include "pan_bg_history.h"
#include "pan_bg_hist_list.h"
#include "pan_bg_rec.h"

/*
   Send is the module that sends large group messages. It uses a timeout
   mechanism to make sure that the sequencer will receive a
   fragment. This is notified by rec_handle, which can only be called
   when the sequence number of a fragment is known, so the sequencer must
   know about it.
*/

#define RETRY_TIMEOUT 10
#define TABLE_SIZE    32

typedef struct{
    int            next;

    int            mode;	/* mode of sending */
    int            dest;	/* destination entry of a fragment */
    int            ack;		/* fragment has been received */

    pan_msg_p      msg;		/* Message to send */
    pan_fragment_p fragment;	/* Fragment corresponding to message */
    int            stored;	/* Current fragment stored in history? */
    int            tick;	/* Time tick to perform action */

    pan_cond_p     cond;	/* condition variable to block sender on */
    int            ready;	/* is message sent? */
    int            flags;	/* waiting for message to end */
}entry_t, *entry_p;

#define WAIT_FLAG 0x01

				/* static variables */
static entry_p     table;
static int         table_size;
static pan_cond_p  table_cond;
static int         table_wait;
static int         free_list;
static int         current_tick;
static pan_nsap_p  send_nsap;

/*
 * send_handler:
 *                 Handles incoming group fragments. Based on the type of
 *                 the fragment, it performs the corresponding actions.
 */

static void
send_handler(pan_fragment_p frag)
{
    pan_fragment_p fragment;
    pan_bg_hdr_p header;

    /* XXX: Always copy, can optimize this? */
    fragment = pan_fragment_create();
    pan_fragment_copy(frag, fragment, 0);
    header = (pan_bg_hdr_p)pan_fragment_header(fragment);
    assert(header->seqno < 1000000); /* XXX: debug */

    /* global synchronization */
    pan_mutex_lock(pan_bg_lock);

    pan_bg_rec_fragment(fragment, header);

    pan_mutex_unlock(pan_bg_lock);
}


void
send_start(void)
{
    int i;

    table_cond = pan_cond_create(pan_bg_lock);

    table_size = 32;
    table = pan_malloc(sizeof(entry_t) * table_size);
    assert(table);
 
    free_list = -1;
    for(i = table_size - 1; i >= 0; i--){
        table[i].next = free_list;

	table[i].mode = -1;
	table[i].dest = -1;
	table[i].ack  = -1;

	table[i].msg      = NULL;
	table[i].fragment = NULL;
        table[i].stored   = 0;
	table[i].tick     = -1;

        table[i].cond = pan_cond_create(pan_bg_lock);
	table[i].ready = 0;
	table[i].flags = 0;

	free_list = i;
    }

    send_nsap = pan_nsap_create();
    pan_nsap_fragment(send_nsap, send_handler, sizeof(pan_bg_hdr_t),
		      PAN_NSAP_UNICAST | PAN_NSAP_MULTICAST);
}

void
send_end(void)
{
    int i;

    pan_nsap_clear(send_nsap);

    for(i = 0; i < table_size; i++){
        pan_cond_clear(table[i].cond);
    }
 
    pan_free(table);
 
    pan_cond_clear(table_cond);

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

    while (free_list == -1){
	table_wait++;
	pan_cond_wait(table_cond);
	table_wait--;
    }

    entry = free_list;
    free_list = table[entry].next;

    table[entry].mode = -1;
    table[entry].dest = -1;
    table[entry].ack  = -1;
    
    table[entry].msg      = NULL;
    table[entry].fragment = NULL;
    table[entry].stored   = 0;
    table[entry].tick     = -1;
 
    table[entry].ready = 0;
    table[entry].flags = 0;

    return entry;
}
 
static void
delete_entry(int entry)
{
    table[entry].next = free_list;
    free_list = entry;

    table[entry].mode = -1;
    table[entry].dest = -1;
    table[entry].ack  = -1;
    
    table[entry].msg      = NULL;
    table[entry].fragment = NULL;
    table[entry].stored   = 0;
    table[entry].tick     = -1;
 
    table[entry].ready = 0;
    table[entry].flags = 0;

    if (table_wait > 0){
	pan_cond_signal(table_cond);
    }
}


static void
send_fragment(int entry, int new)
{
    pan_bg_hdr_p header;
    int flags;

    header = pan_fragment_header(table[entry].fragment);
    flags  = pan_fragment_flags(table[entry].fragment);

    /* header->seqno filled in below */
    header->pid   = pan_my_pid();
    if (new) header->index = index_get();
    header->dest  = table[entry].dest;
    header->src   = entry;
    header->ackno = pan_bg_rseqno - 1; /* XXX check -1 */
    pan_bg_ackno  = pan_bg_rseqno - 1;

    table[entry].ack = 0;
    table[entry].tick = current_tick + RETRY_TIMEOUT;
    if (new) table[entry].stored = 0;

    if (pan_my_pid() != 0){
	if (flags & PAN_FRAGMENT_LAST){
	    /* send fragment to sequencer (platform 0) */
	    header->seqno = SEQNO_PB;
	    pan_comm_unicast_fragment(0, table[entry].fragment);
	}else{
	    /* broadcast message without sequence number */
	    header->seqno = SEQNO_BB;
	    pan_comm_multicast_fragment(pan_bg_all, table[entry].fragment);
	}
    }else{
	table[entry].stored = pan_bg_local(table[entry].fragment, header,
					   table[entry].stored);
    }
}

static void
send_next_fragment(int entry)
{
    entry_p e = &table[entry];

    if (!pan_msg_next(e->msg)){
        e->ready = 1;
        if (e->mode == BG_MODE_ASYNC){
            pan_msg_clear(e->msg);
            e->msg = NULL;
            delete_entry(entry);
        }else if (e->flags & WAIT_FLAG){
            pan_cond_signal(e->cond);
        }
        return;
    }
 
    send_fragment(entry, 1);
}


/*
 * wait_ready:
 *                 Wait until entry gets signalled.
 */
 
static void
wait_ready(int entry)
{
    entry_p e = &table[entry];

    e->flags |= WAIT_FLAG;
    while(e->ready == 0){
        pan_cond_wait(e->cond);
    }
    e->flags &= ~WAIT_FLAG;
 
    delete_entry(entry);
}


/*
 * pan_bg_send_message:
 *                 Send a message to the group. The sending process will
 *                 be blocked until the message has been received
 *                 completely. 
 */

int
pan_bg_send_message(pan_msg_p message, int mode)
{
    int entry;

    pan_mutex_lock(pan_bg_lock);

    entry = find_entry();

    table[entry].mode = mode;
    table[entry].msg = message;
    table[entry].fragment = pan_msg_fragment(message, send_nsap);

    /* send first fragment to anonymous receive map entry */
    table[entry].dest = DEST_ANONYMOUS;

    send_fragment(entry, 1);

    if (mode == BG_MODE_SYNC) wait_ready(entry);

    pan_mutex_unlock(pan_bg_lock);

    return (mode == BG_MODE_SYNC2) ? entry : -1;
}    


void
pan_bg_finish_send(int entry)
{
    pan_mutex_lock(pan_bg_lock);
 
    assert(entry != -1 && table[entry].mode == BG_MODE_SYNC2);
 
    wait_ready(entry);
 
    pan_mutex_unlock(pan_bg_lock);
}

/*
 * send_signal:
 *                 Signal the local arrival of a fragment. Gets as
 *                 parameter the receive map entry of the message.
 */

void
send_signal(int src, int dest)
{
    /* fill in the receive map entry */
    assert(table[src].dest == DEST_ANONYMOUS || table[src].dest == dest);
    table[src].dest = dest;

    send_next_fragment(src);
}
    

/*
 * pan_bg_send_tick
 *                 Tick handler for sending. Every table entry with the
 *                 same tick as current_tick gets its fragment resent.
 */
void
pan_bg_send_tick(int finish)
{
    int i;

    current_tick++;

    for(i = 0; i < table_size; i++) {
	assert(table[i].tick == -1 || table[i].tick >= current_tick);
	if (table[i].tick == current_tick) {
	    assert(table[i].mode != -1);

	    send_fragment(i, 0);
	}
    }
}



/*
 * pan_bg_local:
 *                 A group fragment will be sent from the sequencer
 *                 platform. Add a sequence number immediately.
 */

int
pan_bg_local(pan_fragment_p fragment, pan_bg_hdr_p header, int in_hist)
{
    assert(pan_my_pid() == 0);

    if (!in_hist){
	header->seqno = pan_bg_seqno;
	if (!hist_add(header->seqno, fragment)){
	    pan_bg_warning("Local fragment %d not accepted. Out of history space\n",
		    pan_bg_seqno);
	    return 0;
	}
	pan_bg_seqno++;
    }

    pan_comm_multicast_fragment(pan_bg_all, fragment);

    return 1;
}


