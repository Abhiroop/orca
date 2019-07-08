/*---- with: dedicated Sequencer, meeting Broadcasts, new msg_pool module ----*/

#include <sys/sem.h>
#include <sys/root.h>
#include <sys/link.h>
#include <sys/time.h>
#include <sys/select.h>
#include <assert.h>
#include <string.h>

#include "pan_sys.h"

#include "pan_system.h"
#include "pan_threads.h"
#include "pan_error.h"
#include "pan_malloc.h"
#include "pan_nsap.h"
#include "pan_comm.h"
#include "pan_comm_inf.h"
#include "pan_fragment.h"
#include "pan_bcst_fwd.h"
#include "pan_bcst_snd.h"
#include "pan_msg_cntr.h"
#include "pan_bcst_seq.h"

#include "pan_parix.h"

#include "pan_trace.h"


typedef struct TRC_SEQ_DATA_T {
    int seqno;
    int sender;
} trc_seq_data_t, trc_seq_data_p;

static trc_event_t trc_seq_pb;
static trc_event_t trc_seq_pb_indirect;
static trc_event_t trc_seq_gsb;
static trc_event_t trc_seq_got_data;


#define MAX_SERVER_THREADS 6	/* additional Threads at Sequencer-Node */

static int  g_nr_ready_cpus;

static LinkCB_t **request_in_link[MAX_SERVER_THREADS];
static LinkCB_t **data_in_link[MAX_SERVER_THREADS];

static Semaphore_t local_link_installed; /* synchronize installation of links */

 
#ifdef STATISTICS
#  define STATINC(n)	(++(n))
#  define STATDEC(n)	(--(n))
#else
#  define STATINC(n)
#  define STATDEC(n)
#endif


typedef struct SEQ_STATS {
    int       *pb_direct_request;	/* all senders' direct pb requests */
    int       *pb_indirect_request;	/* all senders' express pb requests */
    int       *gsb_request;		/* all senders' gsb requests */
} seq_stats_t, *seq_stats_p;

static seq_stats_t stats;



/* sequencing threads
 *	- waits for a request with GetMessage
 *	- replies with sequence-number by PutMessage
 *
 *	- counts 'stop_tag' messages till counter == pan_sys_nr
 *	- then: broadcast 'stop_program' message
 *	- and terminate
 */


static int               n_seq_threads;	/* Total # of sequencer threads */
static pan_thread_p     *seq_thread;	/* Sequencer threads */

#define PB_TRIGGER  2

static int               pb_trigger;

static pan_msg_counter_t pb__counter;	/* pb listener */
static Option_t         *request_in_option[MAX_SERVER_THREADS];

static int               pb_counts[MAX_SERVER_THREADS];



/* Data shared between the sequencer threads: */

static Semaphore_t seq_lock;	/* protection for this data */
static int         seqno = 0;
static int         server_id;
static int         leave_id;
static int         nodes_left;


/* multithreaded sequencer: / 2 types of specialized threads:
 *			- request_in threads
 *			- data_in threads
 * communication between the two groups of threads:
 *  - TASK_POP:
 *	data_in threads pop themselves on the 'task' stack and then block
 *	after unblock:
 *	    'dest' contains the processor number of the requesting node
 *	    'dest' being equal -1 is used for termination.
 *  - TASK_PUSH:
 *	request_in threads check the 'task' stack whether there is a free
 *	data_in thread. If so, they unblock this thread. 'id' equals -1,
 *	if there was no free thread.
 */

static int         task_stack_top;
static int         task_stack[MAX_SERVER_THREADS];
static int         task_id[MAX_SERVER_THREADS];
static LinkCB_t   *task_chan[MAX_SERVER_THREADS];
static LinkCB_t   *task_back[MAX_SERVER_THREADS];
static Semaphore_t task_wait[MAX_SERVER_THREADS];


#define TASK_PUSH(dest, chan, back, id) \
	do { \
	    Wait(&seq_lock); \
	    if (task_stack_top >= 0) \
		id = task_stack[task_stack_top--]; \
	    else \
		id = -1; \
	    Signal(&seq_lock); \
	    if (id != -1) { \
		task_id[id] = dest; \
		task_chan[id] = chan; \
		task_back[id] = back; \
		Signal(&task_wait[id]); \
	    } \
	} while (0)


#define TASK_POP(dest, chan, back, my_id) \
	do { \
	    Wait(&seq_lock); \
	    task_stack[++task_stack_top] = my_id; \
	    Signal(&seq_lock); \
	    Wait(&task_wait[my_id]); \
	    dest = task_id[my_id]; \
	    chan = task_chan[my_id]; \
	    back = task_back[my_id]; \
	} while (0)




static void
data_in_thread(void *arg)
{
    int            my_id = (int)arg;
    int            node;
    LinkCB_t      *to_client, *back_to_client;
    int            nummer;
    int            len;
    pan_fragment_p pb_mess;
    mcast_req_t    mcast_reply;
    char           name[32];
#ifdef TRACING
    trc_seq_data_t trc_data;
#endif

    sprintf(name, "seq data in thread %d", my_id);
    trc_new_thread(0, name);

    mcast_reply.tag   = PB_REPLY_TAG;
    mcast_reply.i.seqno = -1;

    pan_msg_counter_hit(&pb__counter);

    for (;;) {
	TASK_POP(node, to_client, back_to_client, my_id);	/* get work */

	if (node == -1)
	    break;		/* termination signal */

	Wait(&seq_lock);
	nummer = seqno++;
	Signal(&seq_lock);
	mcast_reply.i.seqno = nummer;

#ifdef TRACING
	trc_data.seqno = nummer;
	trc_data.sender = node;
#endif
	trc_event(trc_seq_pb_indirect, &trc_data);

							/* send feedback */
	if (SendLink(back_to_client, &mcast_reply, sizeof(mcast_req_t)) !=
		sizeof(mcast_req_t)) {
	    pan_sys_printf("SendLink-Error\n");
	}

	pb_mess = pan_fragment_create();
	len = RecvLink(to_client, pb_mess->data, PACKET_SIZE);

	trc_event(trc_seq_got_data, &trc_data);

	if (len < 0 || len > PACKET_SIZE) {
	    pan_sys_printf("RecvLink-Error\n");
	}

	pan_sys_fragment_nsap_pop(pb_mess, len);

	if (len != pb_mess->size + TOTAL_HDR_SIZE(pb_mess)) {
	    pan_sys_printf("RecvLink-Error\n");
	}

	pan_comm_bcast_snd_new(pb_mess, DATA_MESSAGE, nummer, node,
			       &pb__counter);
	pan_msg_counter_hit(&pb__counter);
    }

    pan_fragment_clear(pb_mess);
    pan_msg_counter_relax(&pb__counter);
}




/* build up links from sequencer to slaves:
 * slaves are the nodes between 'my_start' and 'my_start+my_nodes'
 */
static void
create_seq_links(int my_id, int my_nodes, int my_start)
{
    int         i, error;
    LinkCB_t   *tmp[2];

    request_in_link[my_id]   = pan_calloc(my_nodes, sizeof(LinkCB_t *));
    data_in_link[my_id]      = pan_calloc(my_nodes, sizeof(LinkCB_t *));
    request_in_option[my_id] = pan_calloc(my_nodes * 2, sizeof(Option_t));

    for (i = 0; i < my_nodes; i++) {
	if (i + my_start != pan_sys_Parix_id) {
	    request_in_link[my_id][i] = GetLink(i + my_start, STAR_TOP_1,
						&error);
	    if (request_in_link[my_id][i] == NULL) {
		pan_panic("GetLink failed");
	    }
	    data_in_link[my_id][i] = GetLink(i + my_start, STAR_TOP_2, &error);
	    if (data_in_link[my_id][i] == NULL) {
		pan_panic("GetLink failed");
	    }
	} else {
	    if (LocalLink(tmp) != 0) {
		pan_panic("LocalLink failed");
	    }
	    request_in_link[my_id][i] = tmp[0];
	    pan_bcast_req_link = tmp[1];
	    if (LocalLink(tmp) != 0) {
		pan_panic("LocalLink failed");
	    }
	    data_in_link[my_id][i] = tmp[0];
	    pan_bcast_data_link = tmp[1];

	    Signal(&local_link_installed);
	}
	request_in_option[my_id][i] = ReceiveOption(request_in_link[my_id][i]);
	request_in_option[my_id][i + my_nodes] = request_in_option[my_id][i];
    }
}


static void
break_seq_links(int my_id, int my_nodes)
{
    int         i;

    for (i = 0; i < my_nodes; i++) {
	if (BreakLink(request_in_link[my_id][i]) != 0) {
	    pan_panic("BreakLink failed");
	}
	if (BreakLink(data_in_link[my_id][i]) != 0) {
	    pan_panic("BreakLink failed");
	}
    }
    pan_free(request_in_link[my_id]);
    pan_free(data_in_link[my_id]);
    pan_free(request_in_option[my_id]);
}


static void
clean_up_sequencer(int server_threads)
{
    int            i;
    int            my_id;

    TimeWaitHigh(TimeNowHigh() + 3 * CLK_TCK_HIGH);	/* wait for others to
							 * end too */

    pan_msg_counter_hit(&pan_bcast_state.snd_counter);

    pan_comm_bcast_await(seqno);

    Wait(&pan_comm_req_lock);
    pan_comm_bcast_snd_small(NULL, DATA_MESSAGE | STOP_PROGRAM,
			     seqno, pan_sys_Parix_id,
			     &pan_bcast_state.snd_counter);
    Signal(&pan_comm_req_lock);

    if (pan_sys_link_connected) {
	for (i = 0; i < server_threads; i++) {
	    TASK_PUSH(-1, NULL, NULL, my_id);
	    if (my_id == -1) {
		pan_panic("Data_In_Thread Termination failed");
	    }
	}
    } else {
	for (i = 0; i < server_threads - 1; i++) {
	    pan_comm_send_control_msg(TERM_TAG);
	}
    }
}




static void
sequencer_link_thread(void *arg)
{
    int            server_threads = (int)arg;
    unsigned int   t0;
    unsigned int   t1;
    mcast_req_t    mcast_acpt;
    int            nr_ready_cpus = 0;
    int            my_id;
    int            my_nodes;
    int            my_start;
    int            sel;
    int            fair;
    int            pb_count;
    int            i;
    int            len;
    pan_fragment_p rec_mess;
    bcast_hdr_p    bcast_hdr;
    mcast_req_p    ip;
    boolean        is_express_msg;
    int            my_task_id;
    char           name[32];
#ifdef TRACING
    trc_seq_data_t trc_data;
#endif

    Wait(&seq_lock);		/* draw your ID, # of nodes, ... */
    my_id = server_id++;
    my_nodes = nodes_left / (server_threads - my_id);
    my_start = (pan_sys_total_platforms - nodes_left);
    nodes_left -= my_nodes;
    if (my_nodes <= 0) {		/* superfluous thread,... */
	i = --leave_id;
    }
    Signal(&seq_lock);

    sprintf(name, "seq link thread %d", my_id);
    trc_new_thread(0, name);

    if (my_nodes <= 0) {		/* superfluous thread,... */

	if (i == 0) {
	    clean_up_sequencer(server_threads);	/* last one does the clean up */
	}
	return;
    }

    create_seq_links(my_id, my_nodes, my_start);

    t0 = TimeNowHigh();
    fair = 0;
    pb_count = 0;
    pan_msg_counter_hit(&pb__counter);

    rec_mess = pan_fragment_create();

    for (;;) {
	sel = SelectList(my_nodes, request_in_option[my_id] + fair);
	sel += fair;
	if (sel >= my_nodes) sel -= my_nodes;
	fair += 1;
	if (fair == my_nodes) fair = 0;

	len = RecvLink(request_in_link[my_id][sel], rec_mess->data,
		       PACKET_SIZE);

	is_express_msg = (len == sizeof(mcast_req_t));
	if (is_express_msg) {		/* Express request */

	    if (len < sizeof(mcast_req_t)) {
		pan_sys_printf("express RecvLink-Error\n");
	    }

	    ip = (mcast_req_p)rec_mess->data;

	    if (ip->tag == STOP_TAG) {	/* handle meta-requests */
		assert(ip->i.sender >= 0 &&
		       ip->i.sender < pan_sys_total_platforms);

		nr_ready_cpus++;
		if (nr_ready_cpus == my_nodes) {
		    break;
		}
		continue;
	    }

	} else {

	    pan_sys_fragment_nsap_pop(rec_mess, len);

	    if (len != rec_mess->size + TOTAL_HDR_SIZE(rec_mess)) {
		pan_sys_printf("fragment RecvLink-Error\n");
	    }

	    bcast_hdr = pan_sys_fragment_comm_hdr_pop(rec_mess);
	    ip = &bcast_hdr->mcast_req;
	}


	/* handle broadcast requests: */

	assert(ip->tag == REQUEST_TAG);

					/* does it pay to fetch the data? */
	if (is_express_msg &&
	    				/* allow indirect_pb bcast? */
	    (pan_sys_protocol & PROTO_indirect_pb) &&
					/* close by? */
	    abs((ip->i.sender % pan_sys_DimX) - pan_sys_x) +
		    abs((ip->i.sender / pan_sys_DimX) - pan_sys_y) >
		(pan_sys_DimX + pan_sys_DimY) / 4) {

	    assert(ip->i.sender != pan_sys_Parix_id);

	    TASK_PUSH(ip->i.sender, data_in_link[my_id][sel],
		      request_in_link[my_id][sel], my_task_id);

	    if (my_task_id != -1) { /* is a data_in_thread available? */
		pb_count++;
		STATINC(stats.pb_indirect_request[ip->i.sender]);
		continue;
	    }	/* else, do normal reply */
	}

	Wait(&seq_lock);
	mcast_acpt.i.seqno = seqno++;
	Signal(&seq_lock);

	if ((! is_express_msg) &&		/* we have the data */
	    pan_bcast_state.cong_all < pb_trigger &&	/* no throttle */
	    mcast_acpt.i.seqno < BC_HIS_UPB(&pan_bcast_state)) {
					 	/* pan_comm_bcast won't block */

#ifdef TRACING
	    trc_data.seqno = mcast_acpt.i.seqno;
	    trc_data.sender = ip->i.sender;
#endif
	    trc_event(trc_seq_pb, &trc_data);

	    mcast_acpt.tag = PB_DONE_TAG;
	    pan_comm_bcast_snd_new(rec_mess, DATA_MESSAGE, mcast_acpt.i.seqno,
				   ip->i.sender, &pb__counter);

	    pb_count++;
	    STATINC(stats.pb_direct_request[ip->i.sender]);
	    pan_msg_counter_hit(&pb__counter);

	} else {

#ifdef TRACING
	    trc_data.seqno = mcast_acpt.i.seqno;
	    trc_data.sender = ip->i.sender;
#endif
	    trc_event(trc_seq_gsb, &trc_data);

	    STATINC(stats.gsb_request[ip->i.sender]);
	    mcast_acpt.tag = REPLY_TAG;
	}

#ifdef DETDEBUG
	pan_sys_printf("seq. replies (%d,%d) to %d\n",
		mcast_acpt.i.seqno, mcast_acpt.tag, ip->i.sender);
#endif

						    /* send feedback */
	if (SendLink(request_in_link[my_id][sel], &mcast_acpt,
		     sizeof(mcast_req_t)) != sizeof(mcast_req_t)) {
	    pan_sys_printf("SendLink-Error\n");
	}
    }

    pan_fragment_clear(rec_mess);
    pan_msg_counter_relax(&pb__counter);

    pb_counts[my_id] = pb_count;

    t1 = TimeNowHigh();
/*
    pan_sys_printf("CPUS=%d, SEQ=%d,id %d / %d buffers * %d byte, time %u sec, pb: %d\n",
	      pan_sys_total_platforms, pan_my_pid(), my_id, MAXBUFFER, PACKET_SIZE,
	      (t1-t0)/CLK_TCK_HIGH, pb_count);
*/

    break_seq_links(my_id, my_nodes);

    Wait(&seq_lock);		/* draw your leave-ID */
    i = --leave_id;
    Signal(&seq_lock);
    if (i == 0) {
	clean_up_sequencer(server_threads);	/* last one does the clean up */
    }
}




static void
sequencer_rr_thread(void *arg)
{
    int            server_threads = (int)arg;
    unsigned int   t0, t1;
    mcast_req_t    mcast_req;
    int            nr_ready_cpus = 0;
    int            my_id;
    int            sender, tag;
    int            pb_count, i;
    int            len;
    pan_fragment_p rec_mess;
    RR_Message_t   request_mess;
    char           name[24];
#ifdef TRACING
    trc_seq_data_t trc_data;
#endif

    Wait(&seq_lock);		/* draw your ID, # of nodes, ... */
    my_id = server_id++;
    Signal(&seq_lock);

    sprintf(name, "seq rr thread %d", my_id);
    trc_new_thread(0, name);

    t0 = TimeNowHigh();
    pb_count = 0;
    pan_msg_counter_hit(&pb__counter);
    rec_mess = pan_fragment_create();

    for (;;) {
	tag = GetMessage(-1, REQ_ORMU, REQ_TYPE, -1, &request_mess);
	if (tag < 0)
	    pan_sys_printf("GetMessage-Error\n");

	sender = request_mess.Header.SourceProcId;
	len = request_mess.Header.Size;

	if (tag == STOP_TAG) {	/* handle meta-requests */
	    assert(sender >= 0 && sender < pan_sys_total_platforms);

	    Wait(&seq_lock);
	    nr_ready_cpus = ++g_nr_ready_cpus;
	    Signal(&seq_lock);

	    if (nr_ready_cpus == pan_sys_total_platforms)
		break;
	    continue;
	}
	if (tag == TERM_TAG) {
	    break;
	}
	assert(tag == REQUEST_TAG || tag == SIMPLE_REQUEST_TAG);

	/* handle broadcast requests: */
	Wait(&seq_lock);
	mcast_req.i.seqno = seqno++;
	Signal(&seq_lock);

	if (pan_bcast_state.cong_all < pb_trigger && tag == REQUEST_TAG &&
	    mcast_req.i.seqno < BC_HIS_UPB(&pan_bcast_state)) {
	    mcast_req.tag = PB_DONE_TAG;
	} else {
	    mcast_req.tag = REPLY_TAG;
	}

	if (PutMessage(sender, REQ_ORMU, REQ_TYPE | RR_IS_REPLY,
		       REPLY_CODE, -1, &mcast_req, sizeof(mcast_req_t)) != 0)
	    pan_sys_printf("PutMessage-Error\n");

#ifdef TRACING
	trc_data.seqno = mcast_req.i.seqno;
	trc_data.sender = sender;
#endif

	if (mcast_req.tag == PB_DONE_TAG) {
	    memcpy(rec_mess->data, request_mess.Body, len);

	    trc_event(trc_seq_pb, &trc_data);

	    pan_comm_bcast_snd_new(rec_mess, DATA_MESSAGE, mcast_req.i.seqno,
				   sender, &pb__counter);

	    pb_count++;
	    STATINC(stats.pb_direct_request[sender]);
	    pan_msg_counter_hit(&pb__counter);
	} else {
	    trc_event(trc_seq_gsb, &trc_data);

	    STATINC(stats.gsb_request[sender]);
	}
#ifdef DETDEBUG
	pan_sys_printf("seq. replies (%d,%d) to %d\n",
		mcast_req.i.seqno, mcast_req.tag, sender);
#endif

    }

    pan_fragment_clear(rec_mess);
    pan_msg_counter_relax(&pb__counter);

    pb_counts[my_id] = pb_count;
    Wait(&seq_lock);		/* draw your leave-ID */
    i = --leave_id;
    Signal(&seq_lock);

    if (i == server_threads - 1)
	clean_up_sequencer(server_threads);	/* first one cleans up */

    t1 = TimeNowHigh();

/*
   pan_sys_printf("CPUS=%d, SEQ=%d,id %d / %d buffers * %d byte, time %u sec, pb: %d\n",
	      pan_sys_total_platforms, pan_my_pid(), my_id, MAXBUFFER, PACKET_SIZE,
	      (t1-t0)/CLK_TCK_HIGH, pb_count);
*/
}


static void
init_stats(void)
{
    int n = pan_sys_total_platforms;	/* Abbreviate text */

    stats.pb_direct_request   = pan_calloc(n, sizeof(int));
    stats.pb_indirect_request = pan_calloc(n, sizeof(int));
    stats.gsb_request         = pan_calloc(n, sizeof(int));
}


static void
clear_stats(void)
{
    pan_free(stats.pb_direct_request);
    pan_free(stats.pb_indirect_request);
    pan_free(stats.gsb_request);
}


static void
seq_trc_start(void)
{
    trc_seq_pb          = trc_new_event(3300, sizeof(trc_seq_data_t), "seq pb",
					"seq pb seqno %d sender %d");
    trc_seq_pb_indirect = trc_new_event(3300, sizeof(trc_seq_data_t),
					"seq pb indirect",
					"seq pb/indirect seqno %d sender %d");
    trc_seq_gsb         = trc_new_event(3300, sizeof(trc_seq_data_t),
					"seq gsb", "seq pb seqno %d sender %d");
    trc_seq_got_data    = trc_new_event(3200, sizeof(trc_seq_data_t),
					"seq pb get data",
					"seq pb get data seqno %d sender %d");
}

void
pan_comm_bcast_seq_start(int n_threads)
{
    int i;

    if (pan_sys_Parix_id != pan_sys_sequencer) {
	return;
    }

    if (n_threads <= 0)
	n_threads = 1;
    if (n_threads > MAX_SERVER_THREADS)
	n_threads = MAX_SERVER_THREADS;

    if (pan_sys_protocol & PROTO_pure_GSB) {	/* pure GSB */
	pb_trigger = -1;	/* prevent PB */
    } else
	pb_trigger = PB_TRIGGER;

    g_nr_ready_cpus = 0;
    task_stack_top = -1;
    nodes_left = pan_sys_total_platforms;
    server_id = 0;
    leave_id = n_threads;

    seq_trc_start();

    init_stats();
    pan_msg_counter_init(&pb__counter, 2 * n_threads, "PB");
    pan_comm_info_register_counter(PB_CNT, &pb__counter);

    InitSem(&seq_lock, 1);

    if (pan_sys_link_connected) {
	InitSem(&local_link_installed, 0);

	n_seq_threads = 2 * n_threads;
	seq_thread = pan_calloc(n_seq_threads, sizeof(pan_thread_p));

	for (i = 0; i < n_threads; i++) {
	    seq_thread[2*i] = pan_thread_create(sequencer_link_thread,
						(void*)n_threads,
						STACK, SYSTEM_PRIORITY_H, 0);
	    InitSem(&task_wait[i], 0);
	    seq_thread[2*i+1] = pan_thread_create(data_in_thread, (void*)i,
						  STACK, SYSTEM_PRIORITY_H, 0);
	}

	Wait(&local_link_installed);

    } else {
	n_seq_threads = n_threads;
	seq_thread = pan_calloc(n_seq_threads, sizeof(pan_thread_p));

	for (i = 0; i < n_threads; i++) {
	    seq_thread[i] = pan_thread_create(sequencer_rr_thread,
					      (void*)n_threads,
					      STACK, SYSTEM_PRIORITY_H, 0);
	}
    }
}


void
pan_comm_bcast_seq_end(void)
{
    int i;

    if (pan_sys_Parix_id == pan_sys_sequencer) {
	for (i = 0; i < n_seq_threads; i++) {
	    pan_thread_join(seq_thread[i]);
	}
	pan_msg_counter_clear(&pb__counter);
    }
}



void
pan_comm_bcast_seq_info(void)
{
    int     i;
    int     total_pb_counts;
    int     losts;

    /* The info in stats.pb_requests has previously been deposited by
     * pan_comm_bcast_seq_end(), since its malloc'd data must be cleared in
     * that call. */

    losts = pan_comm_info_get(LOST_SLOT);
    losts -= pan_mallsize(stats.pb_direct_request) / 1024;
    losts -= pan_mallsize(stats.pb_indirect_request) / 1024;
    losts -= pan_mallsize(stats.gsb_request) / 1024;
    pan_comm_info_set(LOST_SLOT, losts);

    pan_comm_info_set(SEQ_THREADS_SLOT, n_seq_threads);

    total_pb_counts = 0;
    for (i = 0; i < n_seq_threads; i++)
	total_pb_counts += pb_counts[i];
    pan_comm_info_set(PB_COUNT, total_pb_counts);

    pan_comm_info_seq_put(stats.pb_direct_request, stats.pb_indirect_request,
			  stats.gsb_request);

    clear_stats();
}
