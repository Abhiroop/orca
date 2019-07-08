/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_bg.h"
#include "pan_bg_send_map.h"
#include "pan_bg_error.h"
#include "pan_bg_global.h"
#include "pan_bg_group.h"
#include "pan_bg_index.h"

/*
   Send is the module that sends large group messages. It uses a timeout
   mechanism to make sure that the sequencer will receive a
   fragment. This is notified by rec_handle, which can only be called
   when the sequence number of a fragment is known, so the sequencer must
   know about it.
*/

typedef struct{
    int        next;
    pan_cond_p cond;		/* condition variable to block sender on */
    int        dest;		/* destination entry of a fragment */
    int        ack;		/* fragment has been received */
    pan_time_p now;		/* Current time */
}entry_t, *entry_p;

/* static variables */
static entry_p     table;
static int         table_size;
static pan_cond_p  table_cond;
static int         table_wait;
static int         free_list;
static pan_time_p  timeout;

void
send_start(void)
{
    int i;

    table_cond = pan_cond_create(group_lock);

    table_size = 5;
    table = pan_malloc(sizeof(entry_t) * table_size);
    assert(table);
 
    free_list = -1;
    for(i = table_size - 1; i >= 0; i--){
        table[i].next = free_list;
        table[i].cond = pan_cond_create(group_lock);
	table[i].dest = -1;
	table[i].ack  = -1;
	table[i].now  = pan_time_create();
        free_list = i;
    }

    timeout = pan_time_create();
    pan_time_set(timeout, 1, 0);
}

void
send_end(void)
{
    int i;

    pan_time_clear(timeout);

    for(i = 0; i < table_size; i++){
        pan_cond_clear(table[i].cond);
	pan_time_clear(table[i].now);
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
 
    return entry;
}
 
static void
delete_entry(int entry)
{
    table[entry].next = free_list;
    free_list = entry;

    if (table_wait > 0){
	pan_cond_signal(table_cond);
    }
}


/*
 * send_message:
 *                 Send a message to the group. The sending process will
 *                 be blocked until the message has been received
 *                 completely. 
 *
 *                 XXX: add timer sweeper and sync2 and async send modes
 */

void
send_group(pan_msg_p message)
{
    pan_fragment_p fragment;    /* current fragment */
    pan_bg_hdr_p header;	/* common header */
    int retrans;
    int stored;
    int flags;
    int entry;

    entry = find_entry();

    fragment = pan_msg_fragment(message, grp_nsap);

    /* send first fragment to anonymous receive map entry */
    table[entry].dest = DEST_ANONYMOUS;

    do{
	header = pan_fragment_header(fragment);
	flags  = pan_fragment_flags(fragment);

	/*
         * Take dest from global variable, because it may have changed in the
         * meantime.
         */
	header->type  = TYPE_FRAGMENT;
	header->pid   = pan_my_pid();
	header->dest  = table[entry].dest;
	header->src   = entry;
	header->index = index_get();

	retrans = 0;
	stored = 0;

	table[entry].ack = 0;
	while(table[entry].ack == 0){
	    /*
	     * assign this inside the loop, because it may change after a
	     * timeout.
	     */

	    header->ackno = rec_seqno - 1; /* XXX check -1 */
	    last_ackno    = rec_seqno - 1;
	    if (pan_my_pid() != 0){
		if (flags & PAN_FRAGMENT_LAST){
		    /* send fragment to sequencer (platform 0) */
		    header->seqno = SEQNO_PB;
		    pan_comm_unicast_fragment(0, fragment);
		}else{
		    /* broadcast message without sequence number */
		    header->seqno = SEQNO_BB;
		    pan_comm_multicast_fragment(pset_all, fragment);
		}
	    }else{
		stored = pan_bg_local(fragment, header, stored);
	    }
	    retrans++;
	    
	    
	    pan_time_get(table[entry].now);
	    pan_time_add(table[entry].now, timeout);
	    (void)pan_cond_timedwait(table[entry].cond, table[entry].now);
	    if (table[entry].ack == 0){
		pan_bg_warning("Retransmitting fragment, retrans: %d seqno: %d\n", 
			retrans, header->seqno);
	    }
	}
    }while(pan_msg_next(message));

    delete_entry(entry);
}    


/*
 * send_signal:
 *                 Signal the local arrival of a fragment. Gets as
 *                 parameter the receive map entry of the message.
 */

void
send_signal(int src, int dest)
{
    table[src].ack = 1;

    /* fill in the receive map entry */
    assert(table[src].dest == DEST_ANONYMOUS || table[src].dest == dest);
    table[src].dest = dest;

    pan_cond_signal(table[src].cond);
}
    
