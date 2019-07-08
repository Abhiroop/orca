/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_mp.h"		/* Provides a message passing interface */
#include "pan_mp_policy.h"
#include "pan_mp_state.h"
#include "pan_mp_error.h"

#include "pan_sys.h"

#include <assert.h>
#include <stdlib.h>

/*
 * TODO: change text
 * The unreliable RPC implementation was state based, meaning that no
 * process had to be associated with the sending of a message. Each time
 * a message was sent, a table entry was acquired in which the state was
 * placed. The first fragment was sent, and from then on the original
 * process that did the send had nothing to do with it. It could block if
 * it wanted to (MODE_SYNC and MODE_SYNC2), but it didn't have to
 * (MODE_ASYNC). Every time an acknowledgement arrived, the next fragment
 * was sent, and every time a timeout occured, the current fragment was
 * sent.
 *
 * With the reliable communication, this protocol has to change, because
 * no acknowledgements are really necessary. For the moment, we made the
 * choice of having an acknowledgement for the first fragment of a large
 * message (i.e. a message with multiple fragments), because this
 * acknowledgement contains information about the destination handle. All
 * further fragments are send to this handle, so there is a tradeoff
 * between this extra acknowledgement and the extra cost of
 * demultiplexing the fragments at the receiving side.
 *
 * The other acknowledgements, however, are eliminated. Therefore, when
 * the first acknowledgement is received, all other fragments have to be
 * sent by some process. My first impression was that the upcall could
 * send all fragments instead of just one, but this implementation blocks
 * the upcall handler for a long time (i.e the time to send all
 * fragments). Another solution is to have a separate sender daemon, that
 * will send all further fragments. The upcall handler can signal this
 * thread when a new message can be sent. Therefore, there is a tradeoff
 * between a context switch and a larger delay of the upcall handler.
 *
 * The sender daemon solution, however, has one additional problem: when
 * should its lock be granted? If the lock is granted during the complete
 * sending phase, the upcall daemon will still be blocked at the lock,
 * so there is no difference between these two implementations. For the
 * moment, the server daemon acquires and releases the lock for each
 * fragment it sends. Since the priority of the server daemon is lower
 * than the priority of the upcall daemon, the upcall can pass it within
 * reasonable time.
 *
 * The server daemon seems to be necessary to avoid a deadlock situation.
 * Image 2 platforms sending large messages to each other. After the
 * acknowledgement is received, both platforms blast their fragments from
 * inside the daemon. However, since both daemons are occupied, they are
 * not ready to receive, so deadlock occurs. This happens on both Amoeba
 * and Parix. Maybe there are some systems in which the fragments are
 * buffered. In that case, the implementation without server daemon may
 * be more efficient?
 */

#ifndef RELIABLE_UNICAST

#include "pan_mp_ticks.h"
#include "pan_mp_conn.h"

#else  /* RELIABLE_UNICAST */


/*
 * The reliable unicast version assumes that the order of the messages is
 * FIFO.
 */


#ifdef SEND_DAEMON
#include "pan_mp_queue.h"

#define QUEUE_STOP   ((state_p)-1) /* Enqueued to stop the daemon thread  */

static pan_thread_p queue_thread; /* daemon thread */
#endif /* SEND_DAEMON */

#endif /* RELIABLE_UNICAST */


/* Types */

/* XXX: Compress this heaer */
/* Koen: changed int into short */
typedef struct{
    short          cpu;		/* platform identifier                   */
    short          handle;	/* destination handle of data            */
#ifndef RELIABLE_UNICAST
#endif
    short          ack_handle;	/* destination handle of acknowledgement */
} destination_t, *destination_p;


typedef struct header{
    destination_t   dest;     /* destination of message      */
    destination_t   src;      /* source of message           */

#ifndef RELIABLE_UNICAST
    seqno_t         seqno;    /* sequence number             */
    seqno_t         ackno;    /* acknowledge sequence number */
#endif

    unsigned char   flags;
#define IMM_ACK_FLAG 0x01     /* waiting for acknowledgement */

}header_t, *header_p;

typedef struct ack_header{
    short int      dest_cpu;
    short int      dest_ack_handle;
    short int      src_cpu;
    short int      src_ack_handle;
#ifndef RELIABLE_UNICAST    
    seqno_t  ackno;
#endif
}ack_header_t, *ack_header_p;


typedef struct state{
    int flags;                   /* state flags                     */
#define STATE_MASK     0x03
#define WAIT_FLAG      0x04

#define STATE_EMPTY    0x00
#define STATE_MAP      0x01
#define STATE_RECEIVE  0x02
#define STATE_SEND     0x03

    int           index;	/* state index number */

    struct state  *next;	/* next entry in single-linked list         */
    int            map;		/* map number to which this entry is linked */
    pan_cond_p     cond;	/* condition variable to wait for message   */

    pan_msg_p      message;	/* the message to be send/received */
    pan_fragment_p fragment;	/* current fragment being sent     */
    header_p       header;	/* current common header           */
    int            offset;	/* current offset of fragment      */

    int            ready;	/* is send/receive ready?          */
    int            mode;	/* mode of communication           */

    destination_t  dest;	/* destination of data/acknowledgement */

#ifndef RELIABLE_UNICAST
    seqno_t        seqno;	/* current sequence number         */

    int            ack;		/* added acknowledgement to piggyback list? */
    header_t       ack_hdr;	/* header of message to be acknowledged     */

    int            tick;	/* tick to perform some action     */
#endif

    int            refcnt;	/* reference counter of this entry */

    /* async message handler */
    void         (*handler)(int map, pan_msg_p msg); 
}state_t, *state_p;



/* Macros */
#define match_destinations(d1, d2) ((d1).cpu == (d2).cpu && (d1).handle == (d2).handle)



/* Constants */
#define KEEP_LOCK    0
#define RELEASE_LOCK 1

#define ANY_CPU    -1
#define ANY_HANDLE -1

#define STATE_CHUNK 100    
    
/* Static variables */
    
static pan_mutex_p  state_lock;
static int          state_finished = 0;

static state_p     *state;	/* Array of state table chunks */
static int          state_nr = 0; /* Nr of state table chunks */
static state_p      free_list = NULL; /* free list of state entries */

static pan_nsap_p   mp_nsap;
static pan_nsap_p   ack_nsap;
static ack_header_t ack_header;

#ifndef RELIABLE_UNICAST
static state_p     *piggy;
static int          current_tick = 0;
static int          num_timers_set;
#endif /* RELIABLE_UNICAST */

#ifdef POLL_ON_WAIT
static int mp_poll = 0;
#endif


#ifdef MP_STATISTICS
static int send_timeout;
#endif


static destination_t no_dest = {ANY_CPU, ANY_HANDLE, ANY_HANDLE};
static destination_t my_dest = {ANY_CPU, ANY_HANDLE, ANY_HANDLE};



/* Forward declarations */
static void upcall(pan_fragment_p message);
static void upcall_direct_ack(void *data);

static void
add_chunk(void)
{
    int curr, i;

    curr = state_nr++;
    state = pan_realloc(state, sizeof(state_p) * state_nr);
    state[curr] = (state_p)pan_malloc(STATE_CHUNK * sizeof(state_t));

    if (curr > 0) pan_mp_warning(STATE_GROW, "State table grown\n");

    for(i = STATE_CHUNK - 1; i >= 0; i--){
	state[curr][i].flags    = STATE_EMPTY;
	state[curr][i].index    = curr * STATE_CHUNK + i;
	state[curr][i].next     = free_list;
	free_list = &state[curr][i];
	state[curr][i].cond     = pan_cond_create(state_lock);
	state[curr][i].message  = NULL;
	state[curr][i].header   = NULL;
	state[curr][i].dest     = no_dest;

#ifndef RELIABLE_UNICAST
	state[curr][i].ack = 0;
	state[curr][i].tick = -1;
#endif

	state[curr][i].refcnt  = 0;
	state[curr][i].handler = NULL;
    }
}

static int
pointer2index(state_p st)
{
    return st->index;
}

static state_p
index2pointer(int id)
{
    state_p st;
    
    st = state[id / STATE_CHUNK];
    st += id % STATE_CHUNK;

    return st;
}


/*
 * add_entry:
 *                 Adds an entry to a specific map. The DIRECT_MAP serves
 *                 as a map for one time communication.
 */

static state_p
add_entry(int map)
{
    state_p map_p = index2pointer(map);
    state_p st;

    assert((map_p->flags & STATE_MASK) == STATE_MAP);
    
    if (!free_list){
	add_chunk();
    }

    st        = free_list;
    free_list = st->next;
    assert(st);

    assert(st->refcnt == 0);

    if (map != DIRECT_MAP){
	/* add entry to map list, so it can be addressed by the map */
	st->next = map_p->next;
	map_p->next = st;
    }else{
	/* Can only reach this entry by its entry number, not its map number */
	st->next = NULL;
    }

    st->map = map;
#ifndef NDEBUG
    st->message = NULL;
    st->header = NULL;
    st->handler = NULL;
#endif
    st->dest = no_dest;
    st->refcnt = 1;
#ifndef RELIABLE_UNICAST
    st->ack = 0;
#endif

    return st;
}


/*
 * remove_entry:
 *                 Remove this entry from the table.
 */

static void
remove_entry(state_p st)
{
    assert(st->flags & STATE_MASK);
    assert(st->refcnt > 0);

    if (--(st->refcnt) == 0){
	assert(st->next == NULL);

	st->flags = STATE_EMPTY;
#ifndef NDEBUG
	st->map = -1;
	st->message = NULL;
	st->header = NULL;
	st->dest = no_dest;
	st->handler = NULL;
#endif

#ifndef RELIABLE_UNICAST
#ifdef CERIEL
	assert(st->tick < current_tick);
#endif
	st->ack = 0;
#endif

	st->next  = free_list;
	free_list = st;
    }
}

/*
 * refer_entry:
 *                 Increment the reference counter of this entry. An
 *                 entry is only cleared when the reference count reaches
 *                 zero.
 */

#ifndef RELIABLE_UNICAST
static void
refer_entry(state_p st)
{
    assert(st->flags & STATE_MASK);
    assert(st->refcnt > 0);
    st->refcnt++;
}
#endif


/*
 * pan_mp_register_map:
 *                 Register a new map entry. A map serves as some sort of
 *                 port, on which multiple recipients may wait. All
 *                 recipients get their own table index, and are linked
 *                 together in a list. When a message for this map entry
 *                 arrives, it is forwarded to the first entry in this
 *                 list. If the list is empty, an error message is generated.
 */

int
pan_mp_register_map(void)
{
    state_p st;

    pan_mutex_lock(state_lock);

    if (!free_list){
	add_chunk();
    }

    st        = free_list;
    free_list = st->next;
    st->flags = STATE_MAP;
    st->next  = NULL;
    st->dest  = no_dest;
    assert(st->refcnt == 0);
    st->refcnt = 1;
    st->handler = NULL;
    st->mode    = 0;
#ifndef RELIABLE_UNICAST
    st->tick = -1;
#endif

    pan_mutex_unlock(state_lock);

    return pointer2index(st);
}


/*
 * pan_mp_free_map:
 *                 Free this map entry. The recipient list must be empty
 *                 (see clear_map).
 */

void
pan_mp_free_map(int index)
{
    state_p st = index2pointer(index);

    pan_mutex_lock(state_lock);

    assert((st->flags & STATE_MASK) == STATE_MAP);
    assert(st->next   == NULL);
    assert(st->refcnt == 1);

    st->flags = STATE_EMPTY;
    st->refcnt = 0;
    st->next = free_list;
    free_list = st;

    pan_mutex_unlock(state_lock);
}


/*
 * pan_mp_clear_map:
 *                 Clear all recipients waiting on this map. If a recieve
 *                 was in asynchronous mode, the clear routine is called
 *                 with the message as argument. Otherwise, the upcall is
 *                 made.
 */

void
pan_mp_clear_map(int index, void (*clear)(pan_msg_p message))
{
    state_p st = index2pointer(index);
    state_p next;

    pan_mutex_lock(state_lock);

    assert((st->flags & STATE_MASK) == STATE_MAP);
    assert(st->refcnt == 1);

    if (st->mode == MODE_ASYNC){
	assert(st->next == NULL);
	/* Problem, upcall handler might not have finished yet.
	 * hack: keep threadswitching until it is done
	 */
	while (st->message == NULL) {
            pan_mutex_unlock(state_lock);
	    pan_thread_yield();
            pan_mutex_lock(state_lock);
	}
	clear(st->message);
	st->message = NULL;
    }else{
	for(st = st->next; st != NULL; st = next){
	    /* Normal receives not handled (yet) */
	    pan_mp_error("No cleanup routine for SYNC or SYNC2 "
			 "mode receives\n");
	    
	    next = st->next;
	    st->next = NULL;
	    
	    remove_entry(st);
	}
	st = index2pointer(index);
	st->next = NULL;
    }

    pan_mutex_unlock(state_lock);
}


#ifndef RELIABLE_UNICAST

/*
 * push:
 *                 Add an entry to the list started at head. Head will be
 *                 updated to contain this new entry.
 */

static void
push(state_p *head, state_p st)
{
    assert(*head != st);
    st->next = *head;
    *head = st;
}


/*
 * pop:
 *                 Remove the first entry from the list started at head.
 *                 Head is updated.
 */

static state_p
pop(state_p *head)
{
    state_p st = *head;

    if (st == NULL) return NULL;
    *head = (*head)->next;
    st->next = NULL;

    return st;
}
    
#endif /* RELIABLE_UNICAST */

/*
 * send_ack:
 *                 Send an acknowledgement based on the entry and the
 *                 header. imm specifies whether the acknowledgement
 *                 should be send immediately or be put in a pending
 *                 acknowledgement queue.
 */

static void
send_ack(state_p st, header_p header, int imm)
{
#ifndef RELIABLE_UNICAST
    assert(st->seqno == header->seqno);
#endif
    assert(match_destinations(header->src, st->dest));

    if (imm){
	ack_header.dest_cpu        = header->src.cpu;
	ack_header.dest_ack_handle = header->src.handle;
	ack_header.src_ack_handle  = pointer2index(st);
#ifndef RELIABLE_UNICAST
	ack_header.ackno           = header->seqno;
#endif
	pan_comm_unicast_small(header->src.cpu, ack_nsap, (void *)&ack_header);
    }
#ifndef RELIABLE_UNICAST
    else{
	st->ack     = 1;
#ifdef CERIEL
    	assert(st->tick < current_tick);
#endif
	st->tick    = current_tick + ACK_TIMEOUT;
	st->ack_hdr = *header;
	refer_entry(st);
	push(&piggy[st->dest.cpu], st);
	num_timers_set++;
    }
#endif

}


/*
 * send_anonymous_ack:
 *                 Send an acknowledgement without knowing an entry
 *                 number. All information is taken from the header. The
 *                 acknowledgement is sent immediately.
 */

#ifndef RELIABLE_UNICAST
static void
send_anonymous_ack(header_p header)
{
    /*
    * Send an acknowledgement to src.cpu, src.handle
    */
    ack_header.dest_cpu        = header->src.cpu;
    ack_header.dest_ack_handle = header->src.handle;
    ack_header.src_ack_handle  = header->dest.handle;
    ack_header.ackno           = header->seqno;

    pan_comm_unicast_small(header->src.cpu, ack_nsap, (void *)&ack_header);
}
#endif



/*
 * send_fragment:
 *                 (re)send the fragment stored at entry entry.
 */

static void
send_fragment(state_p st, int new, int locked)
{
    header_p hdr = st->header;
#ifndef RELIABLE_UNICAST
    state_p ackp;

#ifdef CERIEL
    if (new) assert(st->tick < current_tick);
#endif
    st->tick = current_tick + RETRY_TIMEOUT;
    st->ack = 0;
#endif

    st->flags &= ~STATE_MASK;
    st->flags |= STATE_SEND;

    hdr->dest = st->dest; /* reset because destination may be changed */

    if (new){
	hdr->src = my_dest;
	hdr->src.handle = pointer2index(st);

#ifndef RELIABLE_UNICAST
	hdr->dest.ack_handle = ANY_HANDLE;

	num_timers_set++;
	if ((ackp = pop(&piggy[st->dest.cpu]))){
	    assert(ackp->ack);
	    assert(ackp->ack_hdr.src.cpu == hdr->dest.cpu);
	    assert((ackp->flags & STATE_MASK) == STATE_RECEIVE);
	    assert(ackp->tick != -1);

	    ackp->ack = 0;
	    ackp->tick = -1;
	    num_timers_set--;

	    /*
	    * Send a piggybacked acknowledgement to dest. The
	    * acknowledgement is from entry ack_entry to entry
	    * state[MOD(ack_entry)].ack_hdr.src.handle.
	    */
	    hdr->src.ack_handle  = pointer2index(ackp);
	    hdr->dest.ack_handle = ackp->ack_hdr.src.handle;
	    hdr->ackno           = ackp->seqno;

	    remove_entry(ackp);
	}

	hdr->seqno = pan_mp_conn_get_seqno(hdr->dest.cpu);
	st->seqno = hdr->seqno;
#endif /* RELIABLE_UNICAST */
    }

#ifndef RELIABLE_UNICAST
    assert(hdr->seqno == st->seqno);
#endif

    assert(hdr->src.handle == pointer2index(st));

    if (locked == RELEASE_LOCK){
	/*
	* Avoid a deadlock situation when the receive daemon is already
	* blocked on the state_lock. Since the pan_unicast may block if
	* the receiver is not receiving, a chain can be formed that
	* causes deadlock. 
	*/
	pan_mutex_unlock(state_lock);
    }

    pan_comm_unicast_fragment(hdr->dest.cpu, st->fragment);

    if (locked == RELEASE_LOCK){
	pan_mutex_lock(state_lock);
    }
}



/*
 * send_next_fragment:
 *                 Sends the next fragment for state entry. Returns 1
 *                 if a fragment is sent, 0 if the previous fragment was
 *                 the last fragment.
 */

static int
send_next_fragment(state_p st, int locked)
{
#ifndef RELIABLE_UNICAST
    int flags;
#endif
    if (!pan_msg_next(st->message)){
	st->ready = 1;
	if (st->mode == MODE_ASYNC){
	    pan_msg_clear(st->message);
	    st->message = NULL;
	    remove_entry(st);
	}else if (st->flags & WAIT_FLAG){
	    pan_cond_signal(st->cond);
	}
	
	pan_mp_warning(PROGRESS, NULL);
	
	return 0;
    }

    st->header = (header_p)pan_fragment_header(st->fragment);
#ifdef RELIABLE_UNICAST
    st->header->flags = 0;
#else
    flags = pan_fragment_flags(st->fragment);
    st->header->flags = (flags & PAN_FRAGMENT_LAST) ? 0 : IMM_ACK_FLAG;
#endif

    send_fragment(st, 1, locked);

    return 1;
}


/*
 * handle_ack:
 *                 Handles an acknowledgement. If it acknowledges the
 *                 last fragment, the sender is brought up to date.
 *                 Otherwise, the next fragment is sent.
 */

static void
handle_ack(state_p st, header_p header)
{
#ifndef RELIABLE_UNICAST
    st->ack = 1;
    if (st->tick != -1){
	st->tick = -1;
	num_timers_set--;
    }
#endif

    assert(st->dest.cpu == header->src.cpu);
    assert(st->dest.ack_handle == ANY_HANDLE);

    st->dest.handle = header->src.ack_handle;



#ifdef RELIABLE_UNICAST
    /*
    * With reliable message passing, we need only one acknowledgement to
    * know the destination handle. Since there is no process associated
    * with the sending of fragments, we have to send the remaining
    * fragments some other way. Here, we implement two strategies.
    */

#ifndef SEND_DAEMON
    /*
    * We send all the fragments from inside the system upcall. This may
    * be a performance bottleneck, or may even cause deadlock.
    *
    * The loop ends when the last fragment is sent. 
    *
    * The entry was removed here because there is no acknowledgement for
    * the last fragment. However, I commented it out, because the entry
    * is also cleared in send_next_fragment. How in the world did this
    * run?
    */

    st->header->flags &= ~IMM_ACK_FLAG;
    while(send_next_fragment(st, KEEP_LOCK))
	;

    /*$if (st->mode == MODE_ASYNC){
    remove_entry(st);
    }$*/

#else  /* SEND_DAEMON */

    /*
    * Queue this entry if the message isn't completely sent yet, so that
    * the send daemon can send all remaining fragments.
    */

    if (!(pan_fragment_flags(st->fragment) & PAN_FRAGMENT_LAST)){
	pan_mp_queue_enqueue((void *)st);
    }

#endif /* SEND_DAEMON */

#else  /* RELIABLE_UNICAST */
    (void)send_next_fragment(st, KEEP_LOCK);
#endif /* RELIABLE_UNICAST */

}


/*
 * handle_fragment:
 *                 Handles a new fragment. If this message is the last
 *                 fragment, the assembled message is forwarded to the
 *                 receiver, and a pending acknowledgement is
 *                 registered. If not, an immediate acknowledgement is
 *                 sent.
 */

static void
handle_fragment(state_p st, pan_fragment_p fragment, header_p header, int flags)
{
    void (*handler)(int map, pan_msg_p message);
    pan_msg_p msg;
    int map, imm_ack = 1;

    st->dest = header->src;

#ifndef RELIABLE_UNICAST
    st->seqno = header->seqno;

    if (flags & PAN_FRAGMENT_LAST && !(header->flags & IMM_ACK_FLAG)){
	imm_ack = 0;
    }
    send_ack(st, header, imm_ack);
#else
    if (header->flags & IMM_ACK_FLAG){
	send_ack(st, header, imm_ack);
    }
#endif

    pan_msg_assemble(st->message, fragment, 0);
    
    if (flags & PAN_FRAGMENT_LAST){
	st->ready = 1;
	if (st->flags & WAIT_FLAG){
	    assert(st->mode != MODE_ASYNC);
	    pan_cond_signal(st->cond);
	}else if (st->mode == MODE_ASYNC){
	    msg     = st->message;
	    map     = st->map;
	    handler = st->handler;
	    assert(handler);
	    remove_entry(st);
	    pan_mutex_unlock(state_lock);
	    handler(map, msg);
	    pan_mutex_lock(state_lock);

	    st = index2pointer(map);
	    if (st->message == NULL){
		/* Cache one message for asynchronous upcalls */
		st->message = pan_msg_create();
	    }
	}
    }
}


/*
 * handle_send_timeout:
 *                 A message is not acknowledged on time, so it is resend.
 */

#ifndef RELIABLE_UNICAST
static void
handle_send_timeout(state_p st)
{
    assert((st->flags & STATE_MASK) == STATE_SEND);
    assert(st->ack == 0);

    pan_mp_warning(RETRANS, 
		   "Retransmitting message from entry: %d dest: (%d, %d)"
		   " seqno: %d\n", pointer2index(st), st->dest.cpu, 
		   st->dest.handle, st->header->seqno);
#ifdef MP_STATISTICS
    send_timeout++;
#endif
    st->header->flags |= IMM_ACK_FLAG;
    send_fragment(st, 0, KEEP_LOCK);
}
#endif

/*
 * handle_ack_timeout:
 *                 An acknowledegement could not be piggybacked on
 *                 another message on time, so it is sent explicitly.
 */

#ifndef RELIABLE_UNICAST
static void
handle_ack_timeout(state_p st)
{
    assert((st->flags & STATE_MASK) == STATE_RECEIVE);
    assert(st->ack == 1);
    assert(st->tick != -1);

    pan_mp_debug("Sending explicit ack from entry: %d dest: (%d, %d)"
		 " seqno: %d\n", pointer2index(st), st->dest.cpu, 
		 st->dest.handle, st->ack_hdr.seqno);
    send_ack(st, &st->ack_hdr, 1);
    st->ack = 0;
    st->tick = -1;
    num_timers_set--;
}
#endif


/*
 * find_dest:
 *                 Find the destination of the remaining fragments after
 *                 the acknowledgement of the first fragment is lost. We
 *                 match on the source id of the sender and the sequence
 *                 number.
 */

#ifndef RELIABLE_UNICAST
static state_p 
find_dest(destination_t dest, seqno_t seqno)
{
    state_p st;
    int i;

    for(i = 0; i < state_nr; i++){
	for(st = state[i]; st < &state[i][STATE_CHUNK]; st++){
	    if (st->flags && match_destinations(dest, st->dest) && 
		st->seqno == seqno){
		return st;
	    }
	}
    }

    return NULL;
}
#endif



/*
 * handle_duplicate:
 *                 Handle a duplicate message.
 */

static int
handle_duplicate(header_p header, int flags)
{
#ifndef RELIABLE_UNICAST
    state_p st;
    int entry;

    switch(pan_mp_conn_check_message(header->src.cpu, header->seqno)){
    case CONN_SEQNO_OVERRUN:
	pan_mp_warning(OVERRUN, "Seqno overrun\n");
	return 1;
    case CONN_DUPLICATE:
	entry = header->dest.handle;

	st = index2pointer(entry);
	switch (st->flags & STATE_MASK){
	case STATE_MAP:
	    /* first message to map lost */
	    assert(flags & PAN_FRAGMENT_FIRST);
	    
	    pan_mp_warning(FIRST_LOST, "First message to map lost, "
			   "searching state table\n");
	    if (flags & PAN_FRAGMENT_LAST){
		send_anonymous_ack(header);
		pan_mp_debug("Sent anonymous Ack\n");
	    }else{
		/*
		* fill in the correct entry in the ack message, becasue
		* it will be used by the sender to address the receiver.
		*/
		st = find_dest(header->src, header->seqno);

		if (st){
		    pan_mp_debug("Entry found: %d\n", pointer2index(st));
		    send_ack(st, header, 1);
		}else{
		    pan_mp_debug("Entry not found; ack discarded\n");
		}
	    }
	    break;
	case STATE_RECEIVE:
	    if (flags & PAN_FRAGMENT_LAST){
		/* May be to an old entry */
		send_anonymous_ack(header);
	    }else{
		send_ack(index2pointer(entry), header, 1);
	    }
	    break;
	default:
	    pan_mp_debug("message completely out of line\n");
	    send_anonymous_ack(header);
	}
	return 1;
    case CONN_OK:
	return 0;
    default:
	pan_mp_error("Illegal result conn_table\n");
	return 1;
    }
#endif /* RELIABLE_UNICAST */

    return 0;
}



/*
 * upcall_ack:
 *                 Handle a received acknowledgement.
 */

static void
upcall_ack(header_p header)
{
    state_p st;
    int entry;

    if (header->dest.ack_handle != ANY_HANDLE){
	entry = header->dest.ack_handle;
	st    = index2pointer(entry);

	if ((st->flags & STATE_MASK) == STATE_SEND){
#ifndef RELIABLE_UNICAST
	    if (st->ack == 0){
		if (header->ackno == st->seqno && 
		    st->dest.cpu == header->src.cpu){
		    handle_ack(st, header);
		}else{
		    pan_mp_debug("Ackno %d doesn't match seqno %d\n", 
				   header->ackno, st->seqno);
		}
	    }else{
		pan_mp_debug("Duplicate ack\n");
	    }
#else
	    handle_ack(st, header);
#endif
	}else{
	    pan_mp_debug("Acknowledgement for wrong entry: %d; state: %d\n", 
			   entry, st->flags);
	}
    }
}


/*
 * upcall_direct_ack:
 *                 Handles a direct ack message. A direct ack message is
 *                 sent with pan_nsap_send_small.
 */
static void
upcall_direct_ack(void *data)
{
    ack_header_p hdr = (ack_header_p)data;
/* XXX: Dirty hack */
    header_t header;

    pan_mutex_lock(state_lock);

    if (state_finished){
	pan_mp_warning(TOO_LATE, "direct ack arrived after cleanup");
	pan_mutex_unlock(state_lock);
	return;
    }

    header.dest.cpu        = hdr->dest_cpu;
    header.dest.ack_handle = hdr->dest_ack_handle;
    header.src.cpu         = hdr->src_cpu;
    header.src.handle      = ANY_HANDLE;
    header.src.ack_handle  = hdr->src_ack_handle;
#ifndef RELIABLE_UNICAST
    header.ackno           = hdr->ackno;
#endif

    upcall_ack(&header);
/* Should change upcall_ack to accept individual arguments */

    pan_mutex_unlock(state_lock);
}
    


/*
 * upcall:
 *                 Handle a received fragment. This routine is the entry
 *                 point for fragment sent to mp_nsap.
 */
    
static void
upcall(pan_fragment_p fragment)
{
    header_p header;
    state_p st, map;
    int entry;
    int flags;

    pan_mutex_lock(state_lock);

    if (state_finished){
	pan_mp_warning(TOO_LATE, "Fragment arrived after cleanup");
	pan_mutex_unlock(state_lock);
	return;
    }

    header = (header_p)pan_fragment_header(fragment);
    flags = pan_fragment_flags(fragment);

    upcall_ack(header);

    /* maybe explicit acknowledge message */
    if (header->dest.handle == ANY_HANDLE || handle_duplicate(header, flags)){
	goto end;
    }

    entry = header->dest.handle;
    st    = index2pointer(entry);

    if ((st->flags & STATE_MASK) == STATE_MAP){
	map = st;
	if (st->handler){
	    /* Message for asynchronous map entry */
	    assert(st->next == NULL);

	    /* Build own non-blocking add entry and receive */
	    if (!free_list){
		add_chunk();
	    }
	    st = free_list;
	    assert(st);
	    free_list = st->next;
	    assert(st->refcnt == 0);

	    /* code from add_entry */
	    st->next    = NULL;
	    st->map     = entry;
	    st->dest    = no_dest;
	    st->refcnt  = 1;
#ifndef RELIABLE_UNICAST
	    st->ack     = 0;
#endif

	    /* Code from pan_mp_receive_message */
	    st->flags   = STATE_RECEIVE;
	    st->ready   = 0;
	    st->mode    = MODE_ASYNC;
	    if (map->message){
		st->message  = map->message;
		map->message = NULL;
	    }else{
		st->message = pan_msg_create();
	    }
	    st->handler = map->handler;
	}else{
	    /* forward message to entry */
	    st = st->next;
	    if (st == NULL){ /* no receiver is waiting on this mapped entry */
#ifdef RELIABLE_UNICAST
		pan_mp_error("Discard message because no receivers ready");
#else
		pan_mp_warning(DISCARD,
			       "Discard message because no receivers ready "
			       "map: %d seqno: %d\n", entry, header->seqno);
#endif /* RELIABLE_UNICAST */
		goto end;
	    }
	    
	    map->next = st->next;
	    st->next = NULL;
	}
	assert(st->map == entry);
    }
    
    if ((st->flags & STATE_MASK) != STATE_RECEIVE){
	pan_mp_warning(WRONG_STATE,
		       "Received message while entry %d in state %d\n",
		       pointer2index(st), st->flags);
	goto end;
    }

#ifndef RELIABLE_UNICAST
    if (st->ack){
	pan_mp_debug("Receiving message while ack not sent?\n");
    }
    pan_mp_conn_register_message(header->src.cpu, header->seqno);
#endif

    handle_fragment(st, fragment, header, flags);

  end:
    pan_mutex_unlock(state_lock);
}



/*
 * wait_ready:
 *                 Wait until entry gets signalled.
 */

static void
wait_ready(state_p st)
{
    st->flags |= WAIT_FLAG;
#ifdef POLL_ON_WAIT
    while(st->ready == 0){
	if (mp_poll) {
	    pan_mutex_unlock(state_lock);
	    pan_poll();
	    pan_thread_yield();
	    pan_mutex_lock(state_lock);
	} else {
	    pan_cond_wait(st->cond);
	}
    }
#else
    while(st->ready == 0){
	pan_cond_wait(st->cond);
    }
#endif
    st->flags &= ~WAIT_FLAG;

    /*if ((st->flags & STATE_MASK) == STATE_SEND){
    (void)pan_msg_pop(st->message, sizeof(header_t),
    alignof(header_t));
    }*/

    remove_entry(st);
}


/*
 * pan_mp_send_message:
 *                 Sends a message to the given destination. A
 *                 destination consists of a platform id and an entry
 *                 number. The entry number can be a map number, in which
 *                 case it will be forwarded at the receiving side.
 *                 There are three modes in which a message can be sent:
 *                 MODE_SYNC:  wait until a message is acknowledged
 *                 MODE_SYNC2: continue, but finish_send has to be called
 *                             somewhere in the future.
 *                 MODE_ASYNC: continue. The message will be cleared.
 */

int
pan_mp_send_message(int cpu, int entry, pan_msg_p message, int mode)
{
    state_p st;
#ifdef RELIABLE_UNICAST
    int     is_last_fragment;		/* RFHH */
#endif

    pan_mutex_lock(state_lock);

    st = add_entry(DIRECT_MAP);

    st->flags = STATE_SEND;
    st->ready = 0;
    st->mode = mode;
    st->message = message;

    st->fragment = pan_msg_fragment(st->message, mp_nsap);
    st->header   = (header_p)pan_fragment_header(st->fragment);

    st->dest.cpu        = cpu;
    st->dest.handle     = entry;
    st->dest.ack_handle = ANY_HANDLE;

    st->header->flags = 0;
    if (mode == MODE_SYNC){
	st->header->flags |= IMM_ACK_FLAG;
    }

#ifdef RELIABLE_UNICAST

				/* RFHH: abolish race between this thread and
				 * the send daemon on the fragment flags. */
    is_last_fragment = pan_fragment_flags(st->fragment) & PAN_FRAGMENT_LAST;

    /*
    * Ask for an acknowledgement for the first fragment of a large
    * message (more than one fragments). The acknowledgement contains
    * the table entry where the other fragments can be sent to.
    */
    if (! is_last_fragment){
	st->header->flags |= IMM_ACK_FLAG;
    }

    send_fragment(st, 1, RELEASE_LOCK);
#else
    send_fragment(st, 1, KEEP_LOCK);
#endif

#ifdef RELIABLE_UNICAST
    if (is_last_fragment){	/* RFHH: Here, the race may have occurred */
	/*
	* This case is optimized in the sense that no acknowledgement
	* whatsoever is used. This means that upcall_ack is never
	* called, so the fragment is never cleared.
	*/
	if (st->mode == MODE_ASYNC){
	    /* Remove all state for asynchronous mode */
	    pan_msg_clear(st->message);
	    st->message = NULL;
	    remove_entry(st);
	}
	st->ready = 1;
    }
#endif

    if (mode == MODE_SYNC){
	wait_ready(st);
    }

    pan_mutex_unlock(state_lock);

    return (mode == MODE_SYNC2) ? pointer2index(st) : -1;
}


/*
 * pan_mp_finish_send:
 *                 Wait for send to finish. This is only allowed if a
 *                 message was sent in MODE_SYNC2.
 */

void 
pan_mp_finish_send(int entry)
{
    state_p st;

    pan_mutex_lock(state_lock);

    st = index2pointer(entry);

    assert(entry != -1 && st->mode == MODE_SYNC2);

    st->header->flags |= IMM_ACK_FLAG;

    wait_ready(st);

    pan_mutex_unlock(state_lock);
}


/* 
 * pan_mp_poll_send:
 *                  Poll whether send is finished. This is only allowed 
 *                  if a message was sent in MODE_SYNC2. Returns 1 if 
 *                  ready, 0 otherwise.
 */
int
pan_mp_poll_send(int entry)
{
    state_p st;
    int ret;

    pan_mutex_lock(state_lock);

    st = index2pointer(entry);
    
    assert(entry != -1 && st->mode == MODE_SYNC2);
    
    ret = st->ready;

    pan_mutex_unlock(state_lock);

    return ret;
}

/*
 * pan_mp_receive_message:
 *                 Receive a message. A message can be received in three
 *                 modes:
 *                 MODE_SYNC:   wait for the message to arrive.
 *                 MODE_SYNC2:  continue now, but block later with
 *                              finish_receive. This gives the
 *                              opportunity to register the message.
 *                 MODE_ASYNC:  continue. An asynchronous upcall is made
 *                              to async_handler, getting the map and the
 *                              message as arguments. NOTE: THIS UPCALL
 *                              IS MADE BY THE SYSTEM DAEMON, SO IT
 *                              BETTER NOT BLOCK.
 *    
 */

int
pan_mp_receive_message(int map, pan_msg_p message, int mode)
{
    state_p st;

    assert(mode != MODE_ASYNC);

    pan_mutex_lock(state_lock);

    st = index2pointer(map);
    assert((st->flags & STATE_MASK) == STATE_MAP);

    st = add_entry(map);

    st->flags   = STATE_RECEIVE;
    st->ready   = 0;
    st->mode    = mode;
    st->message = message;
    
    if (mode == MODE_SYNC){
	wait_ready(st);
    }

    pan_mutex_unlock(state_lock);

    return (mode == MODE_SYNC2) ? pointer2index(st) : -1;
}    


/*
 * pan_mp_finish_receive:
 *                 Wait for receive to finish. This is only allowed if a
 *                 message is received in MODE_SYNC2.
 */

void
pan_mp_finish_receive(int entry)
{
    state_p st;

    pan_mutex_lock(state_lock);

    st = index2pointer(entry);

    assert(entry != -1 && st->mode == MODE_SYNC2);

    wait_ready(st);

    pan_mutex_unlock(state_lock);
}

/* 
 * pan_mp_poll_receive:
 *                  Poll whether receive is finished. This is only allowed 
 *                  if a message is received in MODE_SYNC2. Returns 1 if 
 *                  ready, 0 otherwise.
 */
int
pan_mp_poll_receive(int entry)
{
    state_p st;
    int ret;

    pan_mutex_lock(state_lock);

    st = index2pointer(entry);
    
    assert(entry != -1 && st->mode == MODE_SYNC2);
    
    ret = st->ready;

    pan_mutex_unlock(state_lock);

    return ret;
}

/*
 * pan_mp_register_async_receive:
 *                 Registers the asynchronous receive handler.
 */

void
pan_mp_register_async_receive(int map, 
			      void (*handler)(int map, pan_msg_p message))
{
    state_p st = index2pointer(map);

    assert((st->flags & STATE_MASK) == STATE_MAP);
    assert(st->handler == NULL);
    
    st->handler = handler;
    st->mode    = MODE_ASYNC;

    /* Cache one message for asynchronous upcalls */
    st->message = pan_msg_create();
}



/*
 * tick_handler:
 *                 Handles a tick from the tick daemon. It checks if
 *                 there are any pending acknowledgements or messages
 *                 that need to be send (again).
 */


#ifndef RELIABLE_UNICAST
static void
tick_handler(int finish)
{
    int save_num_timers, timers_found = 0;
    int ack_timers = 0, ack_found = 0;
    state_p *head, next, prev, st;
    int i, c;

    current_tick++;
    save_num_timers = num_timers_set;
    
    for(c = 0; c < state_nr; c++){
	for(st = state[c]; save_num_timers > timers_found &&
			       st < &state[c][STATE_CHUNK]; st++){
	    /* Found a registered entry waiting for something */
	    if (st->flags){
		assert(st->tick == -1 || st->tick >= current_tick);
		if (st->tick >= current_tick) timers_found++;

		/* Entry waiting for current tick */
		if (st->tick == current_tick ||
		    (finish && st->tick >= current_tick)){
		    /* perform corresponding action */
		    switch (st->flags & STATE_MASK){
		    case STATE_SEND:
			if (!finish) handle_send_timeout(st);
			break;
		    case STATE_RECEIVE:
			handle_ack_timeout(st);
			ack_timers++;
			break;
		    default:
			pan_mp_error("Cannot handle tick on entry with "
				     "state: %d\n", st->flags);
		    }
		}
	    }
	}
    }
    assert(save_num_timers == timers_found);

    /* remove all acknowledgements that have been handled explicitly */
    for(i = 0; i < pan_nr_platforms() && ack_timers > ack_found; i++){
	head = &piggy[i];
	next = *head;
	while(next){
	    if (next->ack != 0){
		head = &next->next;
		next = next->next;
	    }else{
		*head = next->next;
		prev = next;
		next = next->next;
		prev->next = NULL;
		remove_entry(prev);
		ack_found++;
	    }
	}
    }
    assert(ack_timers == ack_found);

    if (finish || current_tick % 10 == 0){
	/* Dump warnings every second. XXX: todo for reliable unicast */
	pan_mp_dump();
    }
}

#endif /* RELIABLE_UNICAST */

#ifdef POLL_ON_WAIT
void
pan_mp_poll_reply(void)
{
    mp_poll = 1;
}
#endif


#ifdef SEND_DAEMON

/*
 * send_daemon     
 *                 The send daemon is responsible for sending the
 *                 remaining fragments of a large message. All large
 *                 messages are put in a queue, and the send daemon
 *                 extracts them and sends them. Be careful with the
 *                 global lock.
 */

static void
send_daemon(void *arg)
{
    state_p st;

    /*
     * Make sure to have the lock granted while calling any other
     * function.
     */
    pan_mutex_lock(state_lock);
    while ((st = (state_p)pan_mp_queue_dequeue()) != QUEUE_STOP){
	while(send_next_fragment(st, RELEASE_LOCK)){
	    ;
	}
    }
    pan_mutex_unlock(state_lock);

    pan_thread_exit();
}

#endif /* SEND_DAEMON */




/*
 * pan_mp_state_start:
 *                 Start the state module. The state module keeps a table
 *                 of ports to which messages may be sent. There are two
 *                 types of entries, maps and normal entries. Maps serve
 *                 as some sort of multireceive destination, on which
 *                 multiple recipients may wait. There identifiers may be
 *                 published. Normal entries are one time identifiers of
 *                 a destination. There lifetime normally ends after a
 *                 message is received (XXX check this comment)
 */

void
pan_mp_state_start(void)
{
    state_p st;
#ifndef RELIABLE_UNICAST
    int i;
#endif
    
    state_lock = pan_mutex_create();

    add_chunk();

    st = free_list;
    free_list = st->next;

    assert(pointer2index(st) == DIRECT_MAP);

    st->flags = STATE_MAP;
    st->next  = NULL;
#ifndef RELIABLE_UNICAST
    st->tick = -1;
#endif

    my_dest.cpu = pan_my_pid();

    ack_header.src_cpu = pan_my_pid();

#ifndef RELIABLE_UNICAST
    piggy = pan_malloc(pan_nr_platforms() * sizeof(state_p));
    for(i = 0; i < pan_nr_platforms(); i++) piggy[i] = NULL;

    pan_mp_ticks_register(tick_handler, state_lock);
#else  /* RELIABLE_UNICAST */

#ifdef SEND_DAEMON
    pan_mp_queue_lock(state_lock);

    queue_thread = pan_thread_create(send_daemon, NULL, 0, 0, 0);
#endif /* SEND_DAEMON */

#endif /* RELIABLE_UNICAST */

    mp_nsap = pan_nsap_create();
    pan_nsap_fragment(mp_nsap, upcall, sizeof(header_t), PAN_NSAP_UNICAST);
    ack_nsap = pan_nsap_create();
    pan_nsap_small(ack_nsap, upcall_direct_ack, sizeof(ack_header_t),
		   PAN_NSAP_UNICAST);
}


/*
 * pan_mp_state_end:
 *                 Clears up the mess.
 */

void
pan_mp_state_end(void)
{
    int i, c;

    pan_mutex_lock(state_lock);

    pan_nsap_clear(ack_nsap);
    pan_nsap_clear(mp_nsap);

#ifndef RELIABLE_UNICAST
    state_finished = 1;

    pan_mp_ticks_release();	/* Clear ticks daemon and flush acks */

    pan_free(piggy);
#else  /* RELIABLE_UNICAST */

#ifdef SEND_DAEMON
    pan_mp_queue_enqueue(QUEUE_STOP); /* To stop send daemon */
#endif /* SEND_DAEMON */

#endif /* RELIABLE_UNICAST */

    for(c = 0; c < state_nr; c++){
	for(i = 0; i < STATE_CHUNK; i++){
	    pan_cond_clear(state[c][i].cond);
	    if (state[c][i].flags == STATE_MAP && state[c][i].message){
		pan_msg_clear(state[c][i].message);
	    }
	}
	pan_free(state[c]);
	state[c] = NULL;
    }
    pan_free(state);
    state = NULL;

    pan_mutex_unlock(state_lock);

#ifdef SEND_DAEMON
    /* Now the state_lock is released, so wait for send_daemon thread */
    pan_thread_join(queue_thread);
#endif /* SEND_DAEMON */

    /* Can't clear state_lock, because upcalls may come in
    pan_mutex_clear(state_lock);
    */

#ifdef MP_STATISTICS
    printf("Nr send retransmissions: %d\n", send_timeout);
#endif
}

