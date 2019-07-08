#include <assert.h>
#include <limits.h>
#include <sys/list.h>
#include <sys/sem.h>
#include <sys/comm.h>
#include <sys/link.h>
#include <sys/root.h>

#include "pan_sys.h"

#include "pan_trace.h"

#include "pan_global.h"

#include "pan_sys_pool.h"

#include "pan_system.h"
#include "pan_error.h"
#include "pan_malloc.h"

#include "pan_comm.h"
#include "pan_fragment.h"
#include "pan_comm_inf.h"
#include "pan_deadlock.h"
#include "pan_bcst_hst.h"
#include "pan_bcst_fwd.h"
#include "pan_msg_cntr.h"

#include "pan_parix.h"



#define NORMAL_REQUEST (-1)	/* 'seqno'-field for normal update requests */

#define NOTBLOCK       (-1)	/* special value for 'last_blocked' */

#define MARK_DELTA      9

#define METALEN  (sizeof(bcast_hdr_t))

typedef struct TRC_SEND_DATA_T {
    int sender;
    int seqno;
    int size;
} trc_send_data_t, *trc_send_data_p;

typedef struct TRC_RCVE_DATA_T {
    int seqno;
    int size;
} trc_rcve_data_t, *trc_rcve_data_p;

static trc_event_t trc_bcast_rcve;
static trc_event_t trc_bcast_send;


/*---- tuning constants ------------------------------------------------------*/

#define UPDATE_TIMEOUT  13
#define REQUEST_TIMEOUT 17


static pan_thread_p bcast_thread[9];	/* The bcast threads */


/*---- Describe our position in the multiprocessor topology ------------------*/

#define EXTERN_BASE  256

#define DIRECTION(i)	(0x1 << (i))
#define ALL_DIRECTIONS	(0x1 | 0x2 | 0x4 | 0x8)

#define DIR_MARK(i)	(EXTERN_BASE << (i))
#define ALL_MARKS	(ALL_DIRECTIONS << 8)	/* 2^8 == EXTERN_BASE */


#define N_DIRECTIONS 4

#define LEFT    0
#define RIGHT   1
#define DOWN    2
#define UP      3

static int  a_first_straight[N_DIRECTIONS] = {RIGHT, LEFT, UP, DOWN};
static int  a_second_clockwise[N_DIRECTIONS] = {DOWN, UP, RIGHT, LEFT};
static int  a_second_countercw[N_DIRECTIONS] = {UP, DOWN, LEFT, RIGHT};

static int  g_neighbor[N_DIRECTIONS];	/* is there a neighbor in this direction? */
static int  g_neighbor_id[N_DIRECTIONS];	/* processor-id of neighbor in this direction */
static int  g_my_directions;	/* bits of existing directions */
static int  g_my_neighbors;

static int  max_upc_quota;
static int  max_lis_quota;



/*---- private part of the broadcast state -----------------------------------*/
/* Public part is published in our header file (hehe) */

typedef struct PRIV_BCAST_DATA_T {
    Semaphore_t       data_lock;	/* protect history & other data */
    word_t            his_mirror;	/* bit map of history ? */

					/* orderer synchronisation: */
    Semaphore_t       deliver_msg;	/* work for orderer? */
    int               deliver_base;	/* next seqno to be deliv. by orderer */
    int               inter_base;	/* expected seq no of Enqueue/Dequeue */

    Semaphore_t       intern_wait;	/* semaphore to let threads wait on */

    int               shared_control;	/* to <OR> FINAL_STOP in meta-message */

    pan_msg_counter_t lis_counter;	/* quota for listener threads */
    pan_msg_counter_t upc_counter;	/* quota for upcalls */
} priv_bcast_state_t, *priv_bcast_state_p;


/*---- Bit manipulation macros; just within 1 word! --------------------------*/

#define SETBIT(word, bit)    (word) |= (word_t)(0x1 << (bit))
#define TESTBIT(word, bit)   ((word) & (word_t)(0x1 << (bit)))
#define CLEARBIT(word, bit)  (word) &= (word_t)(~(0x1 << (bit)))


/*---- Parse history bitmap --------------------------------------------------*/

#define N_SET(s) \
	(SETBIT(bcast_state.his_mirror, (s - BC_HIS_BASE(&pan_bcast_state))))

#define N_ADVANCE() \
	(bcast_state.his_mirror = (bcast_state.his_mirror >> 1) & 0x7fffffff)



static priv_bcast_state_t bcast_state; /* private part of the broadcast state */

pub_bcast_state_t  pan_bcast_state;	/* public part of the broadcast state */


				/* to indicate kind of task to sender thread */
				    /* 0:  nothing to do  */
				    /* 1:  missed signal  */
				    /* 2, 3:  update, request */
				    /* 8, 9:  send data */
				    /* 10,11:  send data + update, request */
typedef enum TASK_TYPE {
    TASK_no_work	= 0,
    TASK_missed_signal	= (0x1 << 0),
    TASK_update		= (0x1 << 1),
    TASK_send		= (0x1 << 3)
} task_type_t, *task_type_p;


/*---- Information on a neighbour --------------------------------------------*/

typedef struct NEIGHBOUR_T {
    int         index;		/* shared index: listener <-> sender */
    int         counter;	/* shared counter: listener -> sender */
    Semaphore_t work_for;	/* work for senders? */
    task_type_t task_level;
    boolean     is_blocking;	/* mode of direction_sender thread */

    int         mark_base;	/* mark for each direction in the range */
    int         mark_range;	/* BASE to BASE+RANGE */
    int         last_blocked;	/* sequence number of last mark requested */

    int         expects;	/* the 'circ_base' of the neighbor */
    word_t      mirror;		/* bitmap to maintain his history */

				/* Logical timer */
    int         update_timer;	/* send update every UPDATE_TIMEOUT advances */
    int         request_timer;	/* request update every REQ._TIME. recv. msgs */
    boolean     do_update;	/* send an update on this direction */
    boolean     do_request;	/* request an update on this direction */

    int         dir_mark;	/* direction bits of this direction */
} neighbour_t, *neighbour_p;


static neighbour_t neighbour[N_DIRECTIONS];


/*---- find out if sequence number s already received by neighbor i ----------*/

#define N_ALREADY(s, nb) \
	(s < (nb)->expects || \
	    (s < (nb)->expects + BC_HIS_SIZE(&pan_bcast_state) && \
	     TESTBIT((nb)->mirror, (s - (nb)->expects))))





/*---- Statistics ------------------------------------------------------------*/

 
#ifdef STATISTICS
#  define STATINC(n)	(++(n))
#  define STATDEC(n)	(--(n))
#else
#  define STATINC(n)
#  define STATDEC(n)
#endif

typedef struct BCAST_STATISTICS {
    int         slots_ahead;	/* deliver_base - hist.circ_base */
    int         discards;	/* # discarded msgs in Enqueue */
    int         mark_intern_count;
    int         max_cong_all;	/* max on # of items */
} bcast_statistics_t, *bcast_statistics_p;


static bcast_statistics_t stats;



/*---- Statistics on the communication with this neighbour -------------------*/

typedef struct NEIGHBOUR_STATS {
    int         cong_part;	/* # of items in queue for this direction */
    int         max_cong_part;	/* max on # of items for direction */
    int         meta_usage;	/* # of meta-messages sent out on this link */
    int         link_usage;	/* # of messages sent out on this link */
} nb_stats_t, *nb_stats_p;


static nb_stats_t  nb_stats[N_DIRECTIONS];



void
pan_comm_bcast_await(int seqno)
{
    boolean must_wait = FALSE;
    history_item_p b;

    Wait(&bcast_state.data_lock);
    if (BC_HIS_BASE(&pan_bcast_state) < seqno) {
	stats.mark_intern_count++;
	b = BC_HIS_POINTER(&pan_bcast_state, seqno - 1);
	b->waiters++;
	must_wait = TRUE;
    }
    Signal(&bcast_state.data_lock);

    if (must_wait) {
	Wait(&bcast_state.intern_wait);
    }
}




void
pan_sys_neighb_print_state(void)
{
    pan_sys_printf("neighbour hist: %d %d %d %d /L %d %d %d %d /i %d %d %d %d\n",
	neighbour[LEFT].expects, neighbour[RIGHT].expects,
	    neighbour[DOWN].expects, neighbour[UP].expects,
	neighbour[LEFT].last_blocked, neighbour[RIGHT].last_blocked,
	    neighbour[DOWN].last_blocked, neighbour[UP].last_blocked,
	neighbour[LEFT].index, neighbour[RIGHT].index, neighbour[DOWN].index,
	    neighbour[UP].index);
}

void
pan_sys_bcast_print_state(void)
{
    pan_sys_printf("%d %d %d %d %d %d %d %d /is %d %d %d %d\
/c %d%d %d%d %d%d %d%d /cn %d %d %d %d\n",
	bcast_state.deliver_msg.Count, bcast_state.data_lock.Count,
	pan_comm_req_lock.Count, pan_comm_upcall_lock.Count,
	BC_HIS_BASE(&pan_bcast_state), bcast_state.inter_base,
	stats.mark_intern_count, pan_bcast_state.cong_all,
	neighbour[LEFT].is_blocking, neighbour[RIGHT].is_blocking,
		neighbour[DOWN].is_blocking, neighbour[UP].is_blocking,
	neighbour[LEFT].counter, neighbour[LEFT].work_for.Count,
	neighbour[RIGHT].counter, neighbour[RIGHT].work_for.Count,
	neighbour[DOWN].counter, neighbour[DOWN].work_for.Count,
	neighbour[UP].counter, neighbour[UP].work_for.Count,
	nb_stats[LEFT].cong_part, nb_stats[RIGHT].cong_part,
		nb_stats[DOWN].cong_part, nb_stats[UP].cong_part);
}


/* a macro for handing over messages to the orderer: */
/* beware of wrap-around in the history! */

#define SIGNAL_ORDERER() \
	do { \
	    int _lim = bcast_state.inter_base - BC_HIS_BASE(&pan_bcast_state); \
	    \
	    while (_lim++ < BC_HIS_SIZE(&pan_bcast_state) && \
		   ! BC_TEST_HIS(&pan_bcast_state, bcast_state.inter_base, \
				 SLOT_EMPTY | ALL_DIRECTIONS)) { \
		Signal(&bcast_state.deliver_msg); \
		bcast_state.inter_base++; \
		pan_bcast_state.cong_all--; \
	    } \
	} while (FALSE)


/* a macro for signalling only if it is necessary: */
/* one signal is enough. it can serve many tasks. */

#define OR_SIGNAL(nb,b) \
	do { \
	    if ((nb)->task_level == TASK_no_work) { \
		Signal(&(nb)->work_for); \
	    } \
	    (nb)->task_level |= (b); \
	} while (FALSE)


/* a macro for waking up if necessary: */
/* one wake up is enough. */

#define OR_WAKE_UP(nb) \
	do { \
	    if ((nb)->last_blocked != NOTBLOCK)  { \
		(nb)->last_blocked = NOTBLOCK; \
		OR_SIGNAL(nb, TASK_send); \
	    } \
	} while (FALSE)



/* (logical) timer handling:
 * - advance update_timer every time the expected-sequence-number changes
 * - advance request_timer every time you receive a message that has to be forw.
 *   in this direction
 *
 * - reset update_timer when an update is sent (piggybacked or direct)
 * - reset request_timer when an update is received, a request is sent
 *   (piggybacked or direct), or a blocking request is sent
 */


/* reset timer, ask sender to send an update */

#define HANDLE_REQUEST(nb) \
	do { \
	    (nb)->update_timer = UPDATE_TIMEOUT; \
	    if (! (nb)->do_update) { \
		(nb)->do_update = TRUE; \
		OR_SIGNAL(nb, TASK_update); \
	    } \
	} while (FALSE)


/* delete mark, used when blocked partner sends data message */

#define DELETE_MARK(nb) \
	do { \
	    if ((nb)->mark_range != -1) { \
		BC_CLEAR_HIS(&pan_bcast_state, (nb)->mark_base, \
			     (nb)->dir_mark); \
		(nb)->mark_range = -1; \
		(nb)->mark_base = -2; \
	    } \
	} while (FALSE)


static void
update_timer_advance(void)
{
    int         i;
    neighbour_p nb;

    for (i = 0; i < N_DIRECTIONS; i++) {
	nb = &neighbour[i];
	if (g_neighbor[i] && --nb->update_timer < 0) {
	    HANDLE_REQUEST(nb);
	}
    }
}

static void
request_timer_advance(neighbour_p nb)
{

    if (--nb->request_timer < 0) {
	nb->request_timer = REQUEST_TIMEOUT;
	if (! nb->do_request) {
	    nb->do_request = TRUE;
	    OR_SIGNAL(nb, TASK_update);
	}
    }
}


/* Enqueue a message:
 * either forward a message or initiate a new broadcast
 */

static boolean
pan_comm_bcast_enqueue(pan_fragment_p qp, int f_seqno, int f_directions,
		       pan_msg_counter_p x_counter)
{
    pan_fragment_p free_frag;
    history_item_p fp;
    int            i;
    neighbour_p    nb;
    nb_stats_p     nb_stat;

    if (f_seqno < BC_HIS_BASE(&pan_bcast_state)) {   /* message is already in */
	pan_msg_counter_relax(x_counter);
	stats.discards++;
	return FALSE;
    }

    f_directions &= g_my_directions;
    fp = BC_HIS_POINTER(&pan_bcast_state, f_seqno);

    while (f_seqno >= BC_HIS_UPB(&pan_bcast_state)) {	/* cannot enqueue yet */

			/* assert: message comes from an internal source: */
	assert(x_counter != &bcast_state.lis_counter);

	i = 0;
	stats.mark_intern_count++;
	HIS__MARK(fp)++;

	while (TRUE) {
	    Signal(&bcast_state.data_lock);

	    Wait(&bcast_state.intern_wait);  /* wait till msg can be accepted */

	    Wait(&bcast_state.data_lock);
	    if (HIS__TOKEN(fp) &&		/* signal for me and */
		f_seqno >= BC_HIS_UPB(&pan_bcast_state) && /* I can use it */
		HIS__SAMPLE(fp) != f_seqno) {	/* and I am new in this cycle */
		if (HIS__SAMPLE(fp) == INT_MAX) {
						/* I am the first in the cycle*/
		    HIS__SAMPLE(fp) = f_seqno;
		}
	    } else {
		break;
	    }

	    Signal(&bcast_state.intern_wait);
	    assert(stats.mark_intern_count > -1);
	    i++;
	}

	HIS__SAMPLE(fp) = INT_MAX;
	HIS__TOKEN(fp)--;
    }
    assert(f_seqno < BC_HIS_UPB(&pan_bcast_state));

    if (!TEST__HIS(fp, SLOT_EMPTY)) {	/* message is already in */
	pan_msg_counter_relax(x_counter);
	stats.discards++;
	return FALSE;
    }

    for (i = 0; i < N_DIRECTIONS; i++) {
	if (f_directions & DIRECTION(i))
	    if (N_ALREADY(f_seqno, &neighbour[i])) {
						/* message is already there */
		f_directions &= ~DIRECTION(i);
	    }
    }

    				/* extract 'free' message from history */
    free_frag = HIS__MESSAGE(fp);
    assert(free_frag == NULL);
    pan_msg_counter_relax(x_counter);
    BC_PUT_HIS(&pan_bcast_state, f_seqno, qp);	/* put message in history */
    CLEAR__HIS(fp, SLOT_EMPTY);
    SET__HIS(fp, f_directions);
    N_SET(f_seqno);

    pan_bcast_state.cong_all++;
    if (pan_bcast_state.cong_all > stats.max_cong_all)
	stats.max_cong_all = pan_bcast_state.cong_all;

    if (f_seqno == bcast_state.inter_base && f_directions == 0) {
	SIGNAL_ORDERER();	/* advance window */
	return TRUE;
    }

    for (i = 0; i < N_DIRECTIONS; i++) {
	if (f_directions & DIRECTION(i)) {	/* or: signal senders */
	    nb = &neighbour[i];
	    nb->counter++;
	    if (nb->is_blocking) {
		if (f_seqno < nb->expects + BC_HIS_SIZE(&pan_bcast_state) ||
		    f_seqno < nb->last_blocked - MARK_DELTA) {
		    OR_WAKE_UP(nb);
		}
	    } else {
		if (nb->index == INT_MAX) {
		    OR_SIGNAL(nb, TASK_send);
		}
	    }
	    if (nb->index > f_seqno)
		nb->index = f_seqno;

	    nb_stat = &nb_stats[i];
	    nb_stat->cong_part++;
	    if (nb_stat->cong_part > nb_stat->max_cong_part)
		nb_stat->max_cong_part = nb_stat->cong_part;
	    request_timer_advance(nb);
	}
    }

    return TRUE;
}



static void
fill_bcast_hdr(bcast_hdr_p hdr, int control, int seqno, int sender)
{
    hdr->control = control;
    hdr->sender = sender;
    hdr->seqno = seqno;
    if ((pan_sys_x & 1 == pan_sys_y & 1) == (seqno & 1 == 0)) {
	hdr->control |= CLOCKWISE;
    } else {
	hdr->control |= COUNTERCW;
    }
    hdr->row_lim = pan_sys_x;
    hdr->col_lim = pan_sys_y;
}


/* Push the system layer header on the fragment and enqueue it for broadcast.
 * The fragment is lost. */
void
pan_comm_bcast_snd_new(pan_fragment_p frag,
		       int control, int seqno, int sender,
		       pan_msg_counter_p x_counter)
{
    int         is_sent;
    bcast_hdr_p hdr;
#ifdef TRACING
    trc_send_data_t trc_data;

    trc_data.seqno  = seqno;
    trc_data.sender = sender;
    trc_data.size   = frag->size;
#endif
    trc_event(trc_bcast_send, &trc_data);

    hdr = pan_sys_fragment_comm_hdr_push(frag);
    fill_bcast_hdr(hdr, control, seqno, sender);

    do {
	Wait(&bcast_state.data_lock);
	is_sent = pan_comm_bcast_enqueue(frag, seqno, g_my_directions,
					 x_counter);
	Signal(&bcast_state.data_lock);
    } while (! is_sent);
}



/* HANDLE_UPDATE: reset timer, update neighbor->expects, cancel pending request.
 * assert: if a Signal is canceled by TestWait, it addresses the same Semaphore
 *         as used when setting do_request, because the sender thread
 *         resets do_request before changing is_blocking!
 */

static void
handle_update(neighbour_p nb, bcast_hdr_p mh)
{
    int    number;
    word_t mirror;

    number = mh->tag2;		/* select valid pair of values */
    mirror = mh->mirror2;
    if (mh->tag1 != number && mh->mirror1 == mirror) {
	number = mh->tag1;
	mirror = mh->mirror1;
    }

#ifdef NEVER
    if (number < nb->expects)
	pan_sys_printf("number %d , neighbor_e %d (%d %d %d %d)\n",
		  number, nb->expects,
		  mh->mirror1, mh->tag1, mh->mirror2, mh->tag2);
    if (number == nb->expects  &&  nb->mirror & ~mirror)
	pan_sys_printf("mirror %x , neighbor_m %x (%x %d %x %d)\n",
		  mirror, nb->mirror,
		  mh->mirror1, mh->tag1, mh->mirror2, mh->tag2);
#endif

    assert(number >= nb->expects);	/* never go backwards */
    if (number == nb->expects)
	assert((nb->mirror & ~mirror) == 0);

    nb->expects = number;
    nb->mirror = mirror;
    nb->request_timer = REQUEST_TIMEOUT;
    if (nb->do_request) {
	nb->do_request = FALSE;
	if (! (nb->task_level & TASK_send) && ! nb->do_update) {
	    nb->task_level = TASK_no_work;
	    if (TestWait(&nb->work_for) == FALSE) {
		nb->task_level = TASK_missed_signal;
	    }
#ifdef NEVER
pan_sys_printf("CANCEL SIGNAL dir. %d (got update %d-%d) level: %d\n",
i, number, number + BC_HIS_SIZE(&pan_bcast_state), nb->task_level);
#endif
	}
    }
    if (nb->is_blocking && number + BC_HIS_SIZE(&pan_bcast_state) > nb->index) {
	OR_WAKE_UP(nb);
    }
}



static void
handle_mark(neighbour_p nb, int seqno)
{

    if (nb->mark_range != -1) {

    /* assert(seqno < nb->mark_base); */
    /* Due to the improvements of meeting broadcasts and pruning by a bitmap,
     * this assertion does not hold anymore! (only: seqno != mark_base)
     * This may happen: * A installs a mark at B for f.e. seqno x
     *			* A is woken up to forward message/seqno y  (y < x)
     *			* A sees that B already received y
     *			  -> does no send, mark at B is not deleted
     *			* B is ready to receive x and tells this its neigbors
     *			  neighbor C is faster than A and sends x to B
     *			* B announces that it received x
     *			* A considers sending x but sees that B already has x
     *			* A has now to send z (z > x) to B and installs a mark!
     */

	assert(seqno != nb->mark_base);

							    /* delete old */
	BC_CLEAR_HIS(&pan_bcast_state, nb->mark_base, nb->dir_mark);
    }
    assert(seqno >= BC_HIS_UPB(&pan_bcast_state));

    nb->mark_base = seqno - MARK_DELTA;	/* install new */
    if (nb->mark_base < BC_HIS_UPB(&pan_bcast_state)) {
	nb->mark_base = BC_HIS_UPB(&pan_bcast_state);
	HANDLE_REQUEST(nb);
    }
    nb->mark_range = seqno - nb->mark_base;

    assert(nb->mark_range >= 0);

    BC_SET_HIS(&pan_bcast_state, nb->mark_base, nb->dir_mark);
}



/* direction_listener threads
 *	- build link to one neighbor
 *	- listen on this link for messages
 *	- 2 types: = data messages -> forward them
 *		   = meta messages -> notice them
 *
 *	- terminate after receive of 'final_stop' message
 *	  (do not give this message to the orderer)
 */

static void
direction_listener(void *arg)
{
    int            direction = (int)arg;
    int            forw_directions;
    LinkCB_t      *inlink;
    int            error;
    int            len;
    int            cc = 0;
    int            straight_bit;
    int            clockwise_bit;
    int            countercw_bit;
    pan_fragment_p qp;
    pan_fragment_p cp;
    int            is_sent;
    bcast_hdr_p    msgh;
    neighbour_p    nb = &neighbour[direction];

    inlink = GetLink(g_neighbor_id[direction], a_first_straight[direction],
			&error);
    if (inlink == NULL) {
	pan_panic("GetLink failed");
    }

    straight_bit = DIRECTION(a_first_straight[direction]);
    clockwise_bit = DIRECTION(a_second_clockwise[direction]);
    countercw_bit = DIRECTION(a_second_countercw[direction]);

    ChangePriority(SYSTEM_PRIORITY_H);

    pan_msg_counter_hit(&bcast_state.lis_counter);
    qp = pan_fragment_create();

    for (;;) {
	magic_wachhond[direction] = -1;
	len = RecvLink(inlink, qp->data, PACKET_SIZE);
	magic_wachhond[direction] = cc++;
	if (len < 0 || len > PACKET_SIZE) {
	    pan_panic("direction_listener %d: RecvLink failed len = %d",
			direction, len);
	}

	if (len == METALEN) {		/* This is just a meta msg */
	    msgh = (bcast_hdr_p)qp->data;
	    assert(msgh->control & META_MESSAGE);
	} else {
	    pan_sys_fragment_nsap_pop(qp, len);
	    if (len != qp->size + TOTAL_HDR_SIZE(qp)) {
		pan_panic("direction_listener %d: RecvLink failed len = %d size = %d + %d",
			    direction, len, qp->size, TOTAL_HDR_SIZE(qp));
	    }
	    msgh = pan_sys_fragment_comm_hdr_pop(qp);
	}

	if (msgh->control & FINAL_STOP) {
	    break;
	}

	if (msgh->control & DATA_MESSAGE) {
	    assert(msgh->seqno < BC_HIS_UPB(&pan_bcast_state));

	    forw_directions = 0;
	    if (msgh->control & CLOCKWISE) {
		forw_directions |= straight_bit;
		if (msgh->row_lim == pan_sys_x || msgh->col_lim == pan_sys_y)
		    forw_directions |= clockwise_bit;
	    } else {
		assert(msgh->control & COUNTERCW);
		forw_directions |= straight_bit;
		if (msgh->row_lim == pan_sys_x || msgh->col_lim == pan_sys_y)
		    forw_directions |= countercw_bit;
	    }

	    Wait(&bcast_state.data_lock);
	    magic_cookie = direction + 32;

	    handle_update(nb, msgh);

	    DELETE_MARK(nb);
	    if (msgh->control & REQ_UPDATE(straight_bit)) {
		HANDLE_REQUEST(nb);
	    }

	    cp = pan_fragment_create();
	    pan_fragment_copy(qp, cp, 0);

	    is_sent = pan_comm_bcast_enqueue(cp, msgh->seqno, forw_directions,
					     &bcast_state.lis_counter);

	    Signal(&bcast_state.data_lock);

	    if (! is_sent) {
		assert(((pool_entry_p)cp)->pool_mode == OUT_POOL_ENTRY);
		pan_fragment_clear(cp);
	    }

	    pan_msg_counter_hit(&bcast_state.lis_counter);
	} else {
	    assert(msgh->control & META_MESSAGE);

	    Wait(&bcast_state.data_lock);
	    magic_cookie = direction + 64;

	    handle_update(nb, msgh);
	    if (msgh->control & REQ_UPDATE(straight_bit)) {
		if (msgh->seqno == NORMAL_REQUEST ||
			msgh->seqno < BC_HIS_UPB(&pan_bcast_state)) {
		    HANDLE_REQUEST(nb);
		} else {
		    handle_mark(nb, msgh->seqno);
		}
	    }
	    Signal(&bcast_state.data_lock);
	}
    }

    pan_fragment_clear(qp);
    pan_msg_counter_relax(&bcast_state.lis_counter);

    if (BreakLink(inlink) != 0) {
	pan_panic("BreakLink failed");
    }
    ChangePriority(LOW_PRIORITY);
}


/* direction_sender threads
 *	- build up a link to one neighbor
 *	- wait for work or events
 *	- do work: send messages, react on events
 *
 *	- after send of the 'stop_program' message:
 *	- give it to the orderer as usual
 *
 *	- after send of the 'final_stop' message:
 *	  (to cause termination of listener on the link)
 *	  (do not give this message to the orderer)
 *	  terminate
*/

static void
direction_sender(void *arg)
{
    int            direction = (int)arg;
    int            my_bit = DIRECTION(direction);
    int            error;
    int            limit;
    LinkCB_t      *outlink;
    pan_fragment_p qp;
    bcast_hdr_t    meta;
    bcast_hdr_p    msgh;
    boolean        do_updatec;
    boolean        do_requestc;
    boolean        do_send;
    boolean        do_mark;
    task_type_t    level;
    int            indexc;
    int            last_mark_sent;
    boolean        blocking;
    boolean        skip;
    boolean        stop;
    neighbour_p    nb = &neighbour[direction];
    nb_stats_p     nb_stat = &nb_stats[direction];

    outlink = MakeLink(g_neighbor_id[direction], direction, &error);
    if (outlink == NULL) {
	pan_panic("%d: MakeLink in direction %d to node %d failed",
		    pan_my_pid(), direction, g_neighbor_id[direction]);
    }
    ChangePriority(SYSTEM_PRIORITY_H);
    last_mark_sent = INT_MAX;	/* no mark installed at other side */
    stop           = FALSE;
    blocking       = FALSE;
    skip           = FALSE;

    do {
	if (!skip)
	    Wait(&nb->work_for);

	Wait(&bcast_state.data_lock);
	magic_cookie = direction + 8;

	level          = nb->task_level;
	nb->task_level = TASK_no_work;
	do_mark        = FALSE;
	do_send        = FALSE;
	do_updatec     = FALSE;
	do_requestc    = FALSE;
	skip           = FALSE;

	if (level == TASK_no_work) {
	    pan_sys_printf("really fatal error 1\n");
	}
	if (level & TASK_update) {
	    do_updatec = nb->do_update;
	    nb->do_update = FALSE;
	    if (do_updatec)
		nb->update_timer = UPDATE_TIMEOUT;

	    do_requestc = nb->do_request;
	    nb->do_request = FALSE;
	    if (do_requestc)
		nb->request_timer = REQUEST_TIMEOUT;
	}
	if (level & TASK_send) {
	    if (blocking) {
		if (nb->last_blocked == NOTBLOCK) {
		    nb->is_blocking = FALSE;
		    blocking = FALSE;
		} else {
		    pan_sys_printf("really fatal error 2\n");
		}
	    }
	    if (blocking) {
		pan_sys_printf("really fatal error 3\n");
	    }
	    assert(nb->counter > 0);
	    indexc = nb->index;
	    assert(indexc != INT_MAX);

	    limit = 0;
	    while (limit++ < BC_HIS_SIZE(&pan_bcast_state) &&
			!BC_TEST_HIS(&pan_bcast_state, indexc, my_bit)) {
		indexc++;
	    }
	    if (limit > BC_HIS_SIZE(&pan_bcast_state)) {
		pan_sys_printf("really fatal error 4\n");
	    }
	    nb->index = indexc;

	    if (indexc >= nb->expects + BC_HIS_SIZE(&pan_bcast_state)) {
		nb->last_blocked = indexc;
						/* go blocking */
		nb->is_blocking  = TRUE;
		blocking         = TRUE;
		do_mark          = TRUE;
		do_send          = TRUE;
	    } else {
		qp = BC_GET_HIS(&pan_bcast_state, indexc); /* access message */
		do_mark = FALSE;
		do_send = TRUE;

		if (--nb->counter > 0) {	/* keep yourself busy */
		    nb->task_level |= TASK_send;
		    skip = TRUE;
		}
	    }
	    magic_watchdog[direction] = indexc;
	}
	Signal(&bcast_state.data_lock);


	if (do_send && !do_mark) {
	    if (N_ALREADY(indexc, nb))	/* message is already there */
		do_send = FALSE;
	    else {
		msgh = pan_sys_fragment_comm_hdr_look(qp);
		if (do_requestc)
		    msgh->control |= REQ_UPDATE(my_bit);  /* shared write */
		msgh->tag2 = BC_HIS_BASE(&pan_bcast_state); /* shared write */
		msgh->mirror2 = bcast_state.his_mirror;
		msgh->tag1 = BC_HIS_BASE(&pan_bcast_state); /* shared write */
		msgh->mirror1 = bcast_state.his_mirror;

		if (SendLink(outlink, qp->data, qp->size + TOTAL_HDR_SIZE(qp))
			!= qp->size + TOTAL_HDR_SIZE(qp)) {
		    pan_panic("SendLink failed");
		}

		stop = (msgh->control & FINAL_STOP);
		nb_stat->link_usage++;
		last_mark_sent = INT_MAX;
	    }

	    Wait(&bcast_state.data_lock);
	    magic_cookie = direction + 16;

	    BC_CLEAR_HIS(&pan_bcast_state, indexc, my_bit);
	    if (indexc == bcast_state.inter_base &&
		    !BC_TEST_HIS(&pan_bcast_state, indexc, ALL_DIRECTIONS)) {
		SIGNAL_ORDERER();
		nb->index = bcast_state.inter_base;
	    }
	    if (!skip) {
		if (nb->counter > 0) {	/* keep yourself busy */
		    if (nb->task_level == TASK_no_work) {
			skip = TRUE;
		    }
		    nb->task_level |= TASK_send;
		} else
		    nb->index = INT_MAX;
	    }
	    nb_stat->cong_part--;
	    Signal(&bcast_state.data_lock);

	    magic_watchdog[direction] = -1;
	    if (do_send)
		continue;
	}

	if (do_send && last_mark_sent == indexc)
	    do_send = FALSE;
	if (do_send || do_requestc || do_updatec) {
	    meta.control = META_MESSAGE | bcast_state.shared_control;
	    if (do_requestc || do_send)
		meta.control |= REQ_UPDATE(my_bit);
	    if (do_send) {
		meta.seqno = indexc;
		last_mark_sent = indexc;
	    } else {
		meta.seqno = NORMAL_REQUEST;
	    }
	    meta.tag2    = BC_HIS_BASE(&pan_bcast_state);
	    meta.mirror2 = bcast_state.his_mirror;
	    meta.tag1    = BC_HIS_BASE(&pan_bcast_state);
	    meta.mirror1 = bcast_state.his_mirror;

	    if (SendLink(outlink, &meta, METALEN) != METALEN) {
		pan_panic("SendLink failed");
	    }
	    stop = (meta.control & FINAL_STOP);
	    nb_stat->meta_usage++;

	    magic_watchdog[direction] = -1;
	    continue;
	}
    } while (! stop);

    if (BreakLink(outlink) != 0) {
	pan_panic("BreakLink failed");
    }
    ChangePriority(LOW_PRIORITY);
}




#define ADVANCE_CIRC_BASE() \
	do { \
	    int t; \
	    history_item_p hp; \
	    neighbour_p nb; \
	    \
	    hp = BC_HIS_LAST(&pan_bcast_state); \
	    HIS__MESSAGE(hp) = NULL; \
	    SET__HIS(hp, SLOT_EMPTY); \
	    if (TEST__HIS(hp, ALL_MARKS)) { \
		for (t = 0; t < N_DIRECTIONS; t++)  { \
		    nb = &neighbour[t]; \
		    if (nb->mark_base == BC_HIS_UPB(&pan_bcast_state)) { \
			HANDLE_REQUEST(nb); \
			CLEAR__HIS(hp, nb->dir_mark); \
			if (nb->mark_range-- > 0) { \
			    BC_SET_HIS(&pan_bcast_state, \
					BC_HIS_BASE(&pan_bcast_state) + 1, \
					nb->dir_mark); \
			    nb->mark_base++; \
			} else { \
			    nb->mark_base = -3; \
			} \
		    } \
		} \
	    } \
	    if (HIS__MARK(hp)) { \
		HIS__MARK(hp)--; \
		stats.mark_intern_count--; \
		HIS__TOKEN(hp)++; \
		HIS__SAMPLE(hp) = INT_MAX; \
		Signal(&bcast_state.intern_wait); \
	    } \
	    BC_HIS_BASE(&pan_bcast_state)++; \
	    N_ADVANCE(); \
	    update_timer_advance(); \
	} while (FALSE)



/* orderer_thread
 *	- wait for messages to deliver
 *	- make UPCALLs, advance limits, generate events
 *
 *	- terminate on 'stop_program' message
 *	- give this message to the converter_thread
 */

/*ARGSUSED*/
static void
orderer_thread(void *arg)
{
    pan_fragment_p  to_deliver;
    bcast_hdr_p     msgh;
    int             t;
    int             probe;
    int             sender;
    pan_nsap_addr_t nsap_type;
#ifdef TRACING
    trc_rcve_data_t trc_data;
    int             seqno;
#endif

    trc_new_thread(0, "bcast orderer");

    ChangePriority(SYSTEM_PRIORITY_H);

    for (;;) {
	Wait(&bcast_state.deliver_msg);

	Wait(&bcast_state.data_lock);
	magic_cookie = 3;

	assert(!BC_TEST_HIS(&pan_bcast_state, bcast_state.deliver_base,
				SLOT_EMPTY | ALL_DIRECTIONS));

						/* extract message */
	to_deliver = BC_GET_HIS(&pan_bcast_state, bcast_state.deliver_base);
	pan_msg_counter_try_hit(probe, &bcast_state.upc_counter);

	if (probe) {		/* means: there is a free message that can
				 * replace this one */
	    assert(bcast_state.deliver_base == BC_HIS_BASE(&pan_bcast_state));
	    ADVANCE_CIRC_BASE();
	} else {
	    BC_HIS_MK_EMPTY(&pan_bcast_state, bcast_state.deliver_base);
	}

#ifdef TRACING
	seqno = bcast_state.deliver_base;
#endif

	bcast_state.deliver_base++;
	if (bcast_state.deliver_base - BC_HIS_BASE(&pan_bcast_state) >
		stats.slots_ahead)
	    stats.slots_ahead = bcast_state.deliver_base -
						BC_HIS_BASE(&pan_bcast_state);

	Signal(&bcast_state.data_lock);

	/* pan_msg_release_push(to_deliver, upc_relax_function,
				 (void *)to_deliver); */
	msgh = pan_sys_fragment_comm_hdr_look(to_deliver);
	assert(msgh->seqno == bcast_state.deliver_base - 1);
	sender = msgh->sender;
	nsap_type = pan_sys_fragment_nsap_look(to_deliver)->type;

#ifdef TRACING
	trc_data.seqno  = seqno;
	trc_data.size   = to_deliver->size;
#endif
	trc_event(trc_bcast_rcve, &trc_data);

	if (msgh->control & STOP_PROGRAM) {
	    break;
	}

					/* HACK: if we are the dedicated
					 * sequencer, of course do not deliver.
					 * Instead, just clear the fragment.
					 * TO DO:
					 * Let one of the sender threads clear
					 * the fragment. */
	if (pan_sys_Parix_id != pan_sys_sequencer || ! pan_sys_dedicated) {
	    detect_enter(bcast_upcall_);
	    assert(sender == pan_sys_Parix_id || ((pool_entry_p)to_deliver)->pool_mode == OUT_POOL_ENTRY);
	    MAKE_UPCALL(to_deliver);	/* make upcall */
	    assert(sender == pan_sys_Parix_id || ((pool_entry_p)to_deliver)->pool_mode == OUT_POOL_ENTRY);
	    detect_exit(bcast_upcall_);
	}

#ifdef HOME_FRAGMENT_POINTER
	if (sender != pan_sys_Parix_id || nsap_type == PAN_NSAP_SMALL) {
	    assert(((pool_entry_p)to_deliver)->pool_mode == OUT_POOL_ENTRY);
	    pan_fragment_clear(to_deliver);
	}
#else
	assert(((pool_entry_p)to_deliver)->pool_mode == OUT_POOL_ENTRY);
	pan_fragment_clear(to_deliver);
#endif

	if (BC_HIS_BASE(&pan_bcast_state) < bcast_state.deliver_base) {
	    Wait(&bcast_state.data_lock);
	    assert(BC_HIS_MKED_EMPTY(&pan_bcast_state,
					BC_HIS_BASE(&pan_bcast_state)));
	    ADVANCE_CIRC_BASE();
	    Signal(&bcast_state.data_lock);
	} else {
	    pan_msg_counter_relax(&bcast_state.upc_counter);
	}
    }

    Wait(&bcast_state.data_lock);
    magic_cookie = 2;

    bcast_state.shared_control = FINAL_STOP;
    for (t = 0; t < N_DIRECTIONS; t++) {
	HANDLE_REQUEST(&neighbour[t]);
    }
    Signal(&bcast_state.data_lock);

    /* pan_fragment_clear(to_deliver); */

    ChangePriority(LOW_PRIORITY);
}



static void
init_neighbour(neighbour_p nb, int direction)
{
    nb->index = INT_MAX;	/* index in the array */
    nb->counter = 0;
    InitSem(&nb->work_for, 0);

    nb->expects = 0;
    nb->mirror = 0;

    nb->update_timer = UPDATE_TIMEOUT;
    nb->request_timer = REQUEST_TIMEOUT;
    nb->do_update = FALSE;
    nb->do_request = FALSE;
    nb->task_level = TASK_no_work;
    nb->mark_range = -1;
    nb->mark_base = -1;
    nb->last_blocked = NOTBLOCK;
    nb->is_blocking = FALSE;
    nb->dir_mark = DIR_MARK(direction);
}

static void
init_neighbour_stats(nb_stats_p nb)
{
    nb->cong_part = 0;
    nb->max_cong_part = 0;
    nb->meta_usage = 0;
    nb->link_usage = 0;
}


static void
fill_conn_table(void)
{
    int i;

    g_neighbor[LEFT] = (pan_sys_x > 0);
    g_neighbor_id[LEFT] = pan_sys_Parix_id - 1;
    g_neighbor[RIGHT] = (pan_sys_x < pan_sys_DimX - 1);
    g_neighbor_id[RIGHT] = pan_sys_Parix_id + 1;
    g_neighbor[DOWN] = (pan_sys_y > 0);
    g_neighbor_id[DOWN] = pan_sys_Parix_id - pan_sys_DimX;
    g_neighbor[UP] = (pan_sys_y < pan_sys_DimY - 1);
    g_neighbor_id[UP] = pan_sys_Parix_id + pan_sys_DimX;
    g_my_directions = 0;
    g_my_neighbors = 0;
    for (i = 0; i < N_DIRECTIONS; i++) {
	if (g_neighbor[i]) {
	    g_my_directions |= DIRECTION(i);
	    ++g_my_neighbors;
	}
    }
}



static void
init_bcast_state(void)
{
    InitSem(&bcast_state.data_lock, 1);
    pan_bcast_hist_init(&pan_bcast_state.hist);
    bcast_state.his_mirror = 0;

    InitSem(&bcast_state.deliver_msg, 0);
    bcast_state.inter_base = 0;
    bcast_state.deliver_base = 0;

    pan_bcast_state.cong_all = 0;

    InitSem(&bcast_state.intern_wait, 0);

    bcast_state.shared_control = 0;

    /* ???scale and limit the number of buffers; f.e N = 2 CPUS  -->  4, 2, 2 */

    max_lis_quota = g_my_neighbors;

    pan_msg_counter_init(&bcast_state.lis_counter, max_lis_quota, "LISTENER");
    pan_comm_info_register_counter(LISTENER_CNT, &bcast_state.lis_counter);

    max_upc_quota = MIN(pan_sys_total_platforms, 8);

    pan_msg_counter_init(&bcast_state.upc_counter, max_upc_quota, "UPCALL");
    pan_comm_info_register_counter(UPCALL_CNT, &bcast_state.upc_counter);
}



static void
init_stats(void)
{
    stats.discards          = 0;
    stats.slots_ahead       = 0;
    stats.mark_intern_count = 0;
    stats.max_cong_all      = 0;
}



static void
start_threads(void)
{
    int i;
    int n;

    bcast_thread[0] = pan_thread_create(orderer_thread, NULL, STACK,
					SYSTEM_PRIORITY_H, 0);
    if (bcast_thread[0] == NULL) {
	pan_panic("cannot create orderer");
    }

    for (i = 0; i < N_DIRECTIONS; i++) {
	if (g_neighbor[i]) {
	    n = 2 * i + 1;
	    bcast_thread[n] = pan_thread_create(direction_listener, (void*)i,
						STACK, SYSTEM_PRIORITY_H, 0);
	    if (bcast_thread[n] == NULL) {
		pan_panic("cannot create direction listener %d", i);
	    }
	    ++n;
	    bcast_thread[n] = pan_thread_create(direction_sender, (void*)i,
					        STACK, SYSTEM_PRIORITY_H, 0);
	    if (bcast_thread[n] == NULL) {
		pan_panic("cannot create direction sender %d", i);
	    }
	}
    }
}


static void
fwd_trc_start(void)
{
    trc_bcast_rcve = trc_new_event(3400, sizeof(trc_rcve_data_t), "bcast rcve",
				   "bcast rcve seqno %d size %d");
    trc_bcast_send = trc_new_event(3400, sizeof(trc_send_data_t), "bcast send",
				   "bcast from %d seqno %d size %d");
}



void
pan_comm_bcast_fwd_start(void)
{
    int i;

    if (REQ_UPDATE(DIRECTION(N_DIRECTIONS - 1)) > USHRT_MAX) {
	printe("control field does not fit in short int\n");
	abort();
    }

    fwd_trc_start();
    fill_conn_table();

    for (i = 0; i < N_DIRECTIONS; i++) {
	init_neighbour(&neighbour[i], i);
	init_neighbour_stats(&nb_stats[i]);
    }
    init_bcast_state();
    init_stats();
    start_threads();
    if (pan_sys_save_check)
	pan_comm_deadlock_start();
}


void
pan_comm_bcast_fwd_end(void)
{
    int i;
    int n;

    magic_cookie = -1;

    if (pan_sys_save_check) {
	pan_comm_deadlock_end();
    }

    pan_thread_join(bcast_thread[0]);
    for (i = 0; i < N_DIRECTIONS; i++) {
	if (g_neighbor[i]) {
	    n = 2 * i + 1;
	    pan_thread_join(bcast_thread[n]);
	    ++n;
	    pan_thread_join(bcast_thread[n]);
	}
    }

    pan_msg_counter_clear(&bcast_state.lis_counter);
    pan_bcast_hist_clear(&pan_bcast_state.hist);
}


void
pan_comm_bcast_fwd_info(void)
{
    pan_comm_info_set(DISC_SLOT, stats.discards);
    pan_comm_info_set(AHEAD_SLOT, stats.slots_ahead);
    pan_comm_info_set(CONG_SLOT, stats.max_cong_all);
    pan_comm_info_set(META_SLOT, nb_stats[LEFT].meta_usage +
				 nb_stats[RIGHT].meta_usage +
				 nb_stats[DOWN].meta_usage +
				 nb_stats[UP].meta_usage);
}
