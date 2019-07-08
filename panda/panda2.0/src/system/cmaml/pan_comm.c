/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"		/* Provides a system interface */

#include "pan_sync.h"
#include "pan_small.h"
#include "pan_global.h"
#include "pan_comm.h"
#include "pan_message.h"
#include "pan_error.h"
#include "pan_system.h"
#include "pan_threads.h"
#include "pan_nsap.h"
#include "pan_fragment.h"

#include "pan_trace.h"

#ifdef LAM
#include "gam_cmaml.h"
#endif

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define MAX_RBUF           1024              /* available receive buffers */
#define DAEMON_STACK_SIZE 65536              /* bytes */

#define SEQUENCER	      0

#define MAXMACHINE	    128
#define MAXHEIGHT	     14	             /* 2 log MAXMACHINE */
#define BRANCH_FACTOR	      3		     /* includes parent */
#define BRANCH_CHILD	(BRANCH_FACTOR -1 )  /* excludes parent */

typedef void (*handler_t)(int a1, int a2, int a3, int a4);

typedef struct pan_receive {
    pan_nsap_p          r_nsap;           /* destination nsap */
    union {
	int             r_info_data[4];   /* small message data (immediate) */

	struct {            	          /* fragments only */
	    pan_fragment_p rf_frag;       /* fragment pointer */
	    int            rf_ackaddr;    /* ACK address for scopy */ 
	} r_info_frag;
    } r_info;
    pan_receive_p       r_next;           /* queue link */
} pan_receive_t;

#define r_data    r_info.r_info_data
#define r_frag    r_info.r_info_frag.rf_frag
#define r_ackaddr r_info.r_info_frag.rf_ackaddr

typedef struct queue {
    pan_fragment_p q_first;
    pan_fragment_p q_last;
} queue_t;


/* The routing table structure.
 */
typedef struct route {
    int r_branch[BRANCH_FACTOR];
} route_t, *route_p;


/* Macro to pack and unpack two 16-bit integers in a 32-bit integer. */
#define PACK16(src, type)		((src << 16) | type)
#define UNPACK161(src_type) 		((src_type >> 16) & 0xFFFF)
#define UNPACK162(src_type) 		(src_type & 0xFFFF)
#define PACK8(int1, int2)		((int1 << 8) | int2)
#define UNPACK81(int_int)		((int_int >> 8) & 0xFF)
#define UNPACK82(int_int)		(int_int & 0xFF)

static pan_thread_p network_daemon;

/*
 * free_frags is a free list of fragments that can be attached
 * to an rbufs entry (see below).
 */
pan_fragment_p free_frags;

static pan_fragment_p **msg_queue;   /* routing messages */
static queue_t	       *msg_free;    /* queue with free messages */
static queue_t	        msg_process; /* messages to be processed */
static queue_t	        msg_deliver; /* messages to be delivered */

static route_t 		routtab[MAXMACHINE]; 	/* routing table */

static int last_seqno;               /* last sequence number received */
static int global_seqno;             /* sequencer's seqno */

static pan_nsap_p bye_bye_nsap;      /* to send mcast termination msgs */
static int bye_bye_count;

static int num_multisends;           /* #multifragment messages */

/******************************************************************/
/*                 S T A T I S T I C S                            */
/******************************************************************/

int
sys_sprint_comm_stat_size(void)
{
#ifdef STATISTICS
    return 0;
#else
    return 0;
#endif
}

void
sys_sprint_comm_stats(char *buf)
{
#ifdef STATISTICS
#endif
}


/******************************************************************/
/*                   R E C E I V E   Q U E U E                    */
/******************************************************************/

static pan_receive_t rbufs[MAX_RBUF];
static pan_receive_p rbuf_head, rbuf_tail, rbuf_freelist;

static void
rbuf_init(void)
{
    int i;

    for (i = 1; i < MAX_RBUF; i++) {
	rbufs[i].r_next = &rbufs[i-1];
    }
    rbuf_freelist = &rbufs[MAX_RBUF-1];
}

static pan_receive_p
rbuf_alloc(pan_nsap_p nsap)
{
    pan_receive_p rb = rbuf_freelist;

    if (rb) {
	rbuf_freelist = rb->r_next;
	rb->r_next = 0;
	rb->r_nsap = nsap;
    }
    return rb;
}

static void
rbuf_append(pan_receive_p rb)
{
    if (!rbuf_head) {
	rbuf_head = rbuf_tail = rb;
    } else {
	rbuf_tail->r_next = rb;
	rbuf_tail = rb;
    }
}

static pan_receive_p
rbuf_dequeue(void)
{
    pan_receive_p rb = rbuf_head;

    if (rb) {
	if (rbuf_head == rbuf_tail) {
	    rbuf_head = rbuf_tail = 0;
	} else {
	    rbuf_head = rb->r_next;
	}
	rb->r_next = 0;
    }
    return rb;
}

static void
rbuf_free(pan_receive_p rb)
{
    rb->r_next = rbuf_freelist;
    rbuf_freelist = rb;
}

/******************************************************************/
/*                        M E S S A G E S                         */
/******************************************************************/

static void
queue_init(queue_t *q)
{
    q->q_first = q->q_last = 0;
}

static void
append_msg(queue_t *q, pan_fragment_p f)
{
    if (!q->q_first) {
	q->q_first = f;
	q->q_last = q->q_first;
    } else {
	q->q_last->f_next = f;
	q->q_last = f;
    }
}

static void
insert_msg(queue_t *q, pan_fragment_p f)
{
    pan_fragment_p p, r;

    if (!q->q_first) {
	q->q_first = f;
	q->q_last = q->q_first;
    } else {
	r = 0;
	for (p = q->q_first; p; p = p->f_next) {
	    if (f->f_seqno <= p->f_seqno) {	/* insert? */
		if (!r) {
		    f->f_next = q->q_first;
		    q->q_first = f;
		} else {
		    f->f_next = p;
		    r->f_next = f;
		}
		return;
	    }
	    r = p;
	}
	append_msg(q, f);	/* append to the end */
    }
}
		
static pan_fragment_p
dequeue_msg(queue_t *q)
     /* Dequeue first message from queue. */
{
    pan_fragment_p f = q->q_first;
    
    if (f) {
	if (q->q_first == q->q_last) {
	    q->q_first = q->q_last = 0;
	} else {
	    q->q_first = f->f_next;
	}
	f->f_next = 0;
    }
    return f;
}

static pan_fragment_p
alloc_msg_node(int node)
    /* Allocate a free message. Return null if no free message is available. */
{
    assert(0 <= node && node <= pan_sys_nr);
    return(dequeue_msg(&msg_free[node]));
}

static void
free_msg(pan_fragment_p f, int node)
    /* Return m to its free pool of messages. */
{
    assert(0 <= node && node <= pan_sys_nr);

    append_msg(&msg_free[node], f);
}

static void
msg_init(void)
    /* Initialize the free message queues. */
{
    int i, j, nfrags;
    int nq = pan_sys_nr + 1;

    queue_init(&msg_process);
    queue_init(&msg_deliver);

    msg_queue = malloc(nq * sizeof(pan_fragment_p *));
    if (!msg_queue) {
	CMMD_error("msg_init: malloc failed\n");
    }
    
    msg_free = malloc(nq * sizeof(queue_t));
    if (!msg_free) {
	CMMD_error("msg_init: malloc failed\n");
    }
    
    for (j = 0; j < nq; j++) {
	nfrags = (j == pan_sys_nr ? MAX_RBUF : MAXHEIGHT);

	msg_queue[j] = malloc(nfrags * sizeof(pan_fragment_p));
	if (!msg_queue[j]) {
	    CMMD_error("msg_init: malloc failed\n");
	}
	queue_init(&msg_free[j]);
	for (i = 0; i < nfrags; i++) {
	    msg_queue[j][i] = pan_fragment_create();
	    msg_queue[j][i]->f_root = -1;
	    append_msg(&msg_free[j], msg_queue[j][i]);
	}
    }
}

/******************************************************************/
/*                P O I N T - T O - P O I N T                     */
/******************************************************************/

#define NO_RPORT    -1
#define NO_FRAGMENT -2
#define NO_SLOT     -3
#define NO_REPLY    -4
#define SOME_REPLY (NO_REPLY + 1)
#define SCOPY_ACK   -5

static void
overload_msg(char *func, int dest, int why)
{
    static int n;
 
    if (++n == pan_comm_winterval) {
        n = 0;
        printf("%3d: %s: processor %d overloaded: ", pan_sys_pid, func, dest);
	switch (why) {
	case NO_RPORT:
	    printf("no rport available\n");
	    break;
	case NO_FRAGMENT:
	    printf("no fragment available\n");
	    break;
	case NO_SLOT:
	    printf("no buffer slot available\n");
	    break;
	default:
	    CMMD_error("%3d: overload_msg: illegal why value (%d)\n",
		       pan_sys_pid, why);
	}
    }
}

static void
frag_setup(pan_fragment_p f, pan_receive_p rb, pan_nsap_p nsap, int size,
	   int src, int root, int seqno)
{
    int data_size = size - (nsap->hdr_size + FRAG_HDR_SIZE);

    assert(f);
    assert(f->owner);

    /*
     * If the current data buffer is too small to hold all incoming data,
     * then allocate a larger one. Do not use pan_realloc() to avoid
     * copying unused data.
     */
    if (size > f->f_bufsize) {
	f->f_bufsize = size;
	pan_free(f->data);
	f->data = pan_malloc(size);
    }

    f->header      = (frag_hdr_p)(f->data + data_size);
    f->size        = data_size;
    f->f_size      = size;
    f->nsap        = nsap;
    f->owner       = 1;
    f->f_bufent    = rb;
    f->f_src       = src;
    f->f_root      = root;
    f->f_seqno     = seqno;
}

#ifndef LAM
static int
alloc_port(void *buf, int size,
	   void (*handler)(int rport, void *arg), void *arg2)
{
    int rport = CMAML_allocate_rport();

    if (rport >= 0) {
	/*
	 * Initialise the rport.
	 */
	CMAML_set_rport_addr(rport, buf);
	CMAML_set_rport_byte_counter(rport, size);
	CMAML_set_rport_total_nbytes(rport, size);
	CMAML_set_rport_handler(rport, (void *)handler);
	CMAML_set_rport_handler_arg2(rport, arg2);
    }
    return rport;
}
#endif

static void
handle_rport(int rport, int rport_int, int a2, int a3)
{
    *(int *)rport_int = rport;
}

static void
handle_data(int rport, void *arg)
{
    pan_receive_p rb = (pan_receive_p)arg;
    pan_fragment_p f = rb->r_frag;

    if (CMAML_free_rport(rport) == FALSE) {
	CMMD_error("handle_data: rport already free\n");
    }

    if (f->f_seqno == -1) {
	rbuf_append(rb);                /* this is a unicast msg. */
    } else {
	append_msg(&msg_process, f);    /* this is a multicast msg. */
    }

    if (!(f->header->flags & PAN_FRAGMENT_LAST)) {
	CMAML_rpc(f->f_src, handle_rport, SCOPY_ACK, rb->r_ackaddr, 0, 0);
    }
}

static void
handle_send_request(int src_root_nsap, int seqno, int size, int rport_int)
{
    int rport, src_root, src, root, nsap_id;
    pan_nsap_p nsap;
    pan_receive_p rb;
    pan_fragment_p f;

    src_root = UNPACK161(src_root_nsap);
    nsap_id  = UNPACK162(src_root_nsap);
    src      = UNPACK81(src_root);
    root     = UNPACK82(src_root);

    nsap = pan_sys_nsap_id(nsap_id);
    assert(nsap && nsap->type == PAN_NSAP_FRAGMENT);

    /* 
     * We need three things:
     * 1. An empty buffer entry ( rbuf_alloc(nsap) != 0           )
     * 2. A fragment.           ( alloc_msg_node(pan_sys_nr) != 0 )
     * 3. An rport.             ( alloc_port(...) >= 0            )
     */
    if (!(rb = rbuf_alloc(nsap))) {
	CMAML_reply(src, handle_rport, NO_SLOT, rport_int, 0, 0);
	return;
    }
    if (!(f = alloc_msg_node(root))) {
	rbuf_free(rb);
	CMAML_reply(src, handle_rport, NO_FRAGMENT, rport_int, 0, 0);
	return;
    }

    frag_setup(f, rb, nsap, size, src, root, seqno);

    if ((rport = alloc_port(f->data, size, handle_data, rb)) == -1) {
	rbuf_free(rb);
	free_msg(f, root);
	CMAML_reply(src, handle_rport, NO_RPORT, rport_int, 0, 0);
	return;
    }

    rb->r_frag = f;
    rb->r_ackaddr = rport_int; 
    CMAML_reply(src, handle_rport, rport, rport_int, 0, 0);
}

/*
 * Do_send() scopies message data from one node to another.
 * Make sure no other thread gets scheduled while you are
 * in this routine. The scheduler is not fair, so you might
 * never get back here and deadlock.
 */
static void
do_send(char *str, int dst, pan_fragment_p f)
{
    volatile int rport;

    assert(dst != pan_sys_pid);
	
    do {
	rport = NO_REPLY;
	CMAML_request(dst, handle_send_request,
		      PACK16(PACK8(pan_sys_pid, f->f_root), f->nsap->nsap_id),
		      f->f_seqno, f->f_size, (int)&rport);
	CMAML_poll_while_lt(&rport, SOME_REPLY);   /* wait for rport id */

	if (rport < 0) {
	    overload_msg(str, dst, rport);
	}
    } while (rport < 0);

    if (CMAML_scopy(dst, rport, 0, 0, f->data, f->f_size, 0, 0) != TRUE) {
	CMMD_error("pan_comm_unicast_fragment: scopy failed");
    }

    /*
     * The network is _not_ FIFO, so we use stop-and-wait for
     * all but the last fragment.
     */
    if (!(f->header->flags & PAN_FRAGMENT_LAST)) {
	num_multisends++;
	do {
	    CMAML_poll();
	} while (rport != SCOPY_ACK);
    }
}

/******************************************************************/
/*                 U P C A L L   D A E M O N                      */
/******************************************************************/

static void
handle_message(pan_receive_p msg)
{
    pan_fragment_p rcv_frag;
    pan_nsap_p nsap;

    nsap = msg->r_nsap;
    if (!nsap) {                /* nsap == 0  =>  kill message */
	pan_thread_exit();
	CMMD_error("comm_daemon: pan_thread_exit returned\n");
    }
    
    /*
     * Dispatch on message type: either an AM or a fragment.
     */
    switch(nsap->type) {
    case PAN_NSAP_SMALL:
	(*nsap->rec_small)(msg->r_data);
	break;
    case PAN_NSAP_FRAGMENT:
	rcv_frag = msg->r_frag;
	assert(rcv_frag->f_bufent == msg);
	
	(*nsap->rec_frag)(rcv_frag);
	rcv_frag->f_bufent = 0;

	/*
	 * When this fragment was allocated in response to a
	 * remote request, then we return it to the receive
	 * pool.
	 */
	if (rcv_frag->f_src != pan_sys_pid) {
	    free_msg(rcv_frag, rcv_frag->f_root);
	}
	msg->r_frag = 0;
	break;
    default:
	CMMD_error("comm_daemon: illegal nsap type\n");
    }
    
    /*
     * Now release the buffer slot.
     */
    msg->r_nsap = 0;
    rbuf_free(msg);
}

static void
comm_daemon(void *arg)
{
    pan_receive_p rb;

    trc_new_thread( 0, "communication daemon");

    for (;;) {
	pan_poll();
	if ((rb = rbuf_dequeue()) != 0) {
	    do {
		handle_message(rb);    /* we got something; process it */
	    } while ((rb = rbuf_dequeue()) != 0);
	} else {
	    pan_thread_schedule();      /* no messages; yield */
	}
    }
}

/******************************************************************/
/*                      U N I C A S T I N G                       */
/******************************************************************/

static void
reject(handler_t handler, int a0, int a1, int a2, int a3)
{
    printf("%d) rejected Active Message [%p(%d, %d, %d, %d)]\n",
	   pan_sys_pid, handler, a0, a1, a2, a3);
}

/*
 * This is called by one of the mini dispatchers in pan_small.s. To avoid
 * putting the nsap index in a small Active Message, we create a small,
 * fixed number of handlers, one per small nsap. Each of these handlers
 * branches to this routine and adds its nsap index (ni) to the parameters.
 */
void
pan_small_enqueue(int a0, int a1, int a2, int a3, int i)
{
    pan_receive_p rb;

    assert(i == -1 || (0 <= i && i < pan_small_nr));

    rb = rbuf_alloc(i == -1 ? 0 : pan_nsap_map[i]);
    if (!rb) {
	reject( (i == -1 ? 0 : (handler_t)PAN_SMALL_HANDLER(i)),
	       a0, a1, a2, a3);                   /* save it for later */
    }

    /*
     * We're inside an Active Message handler, so we can't make an
     * upcall here. Instead, we enqueue the data.
     */
    rb->r_data[0] = a0;
    rb->r_data[1] = a1;
    rb->r_data[2] = a2;
    rb->r_data[3] = a3;

    rbuf_append(rb);
}

#ifdef FLOW_CONTROL
/******************************************************************/
/*                    F L O W   C O N T R O L                     */
/******************************************************************/

#define MAX_DELAYED  128

static int num_delayed, delay_in, delay_out;

static struct {
    int d_src;
    int d_addr;
} delayed[MAX_DELAYED];

static void handle_seqno(int seqno, int seqno_int, int a2, int a3);

static void
delay_seqno(int src, int addr)
{
    if (num_delayed == MAX_DELAYED) {
	CMMD_error("%d: delay_seqno: delay queue full\n", pan_sys_pid);
    }
    delayed[delay_in].d_src = src;
    delayed[delay_in].d_addr = addr;
    if (++delay_in == MAX_DELAYED) {
	delay_in = 0;
    }
    num_delayed++;
}

static void
delay_flush(void)
{
    if (num_delayed > 0) {
	global_seqno++;
	CMAML_request(delayed[delay_out].d_src, handle_seqno, global_seqno,
		      delayed[delay_out].d_addr, 0, 0);
	if (++delay_out == MAX_DELAYED) {
	    delay_out = 0;
	}
	num_delayed--;
    }
}

#endif

/******************************************************************/
/*                   M U L T I C A S T I N G                      */
/******************************************************************/

/* 
 * Program measuring various (unordered) broadcasting strategies.
 * 	1. Simple: send message to all destinations sequentially.
 *	2. Divide and conquer
 *      3. Tree: setup tree with some branching factor and send message
 *         along tree; forward on all branches, except on the one from
 *	   which the message is coming. (There no cycles, so this works.)
 *	4. Totally-ordered broadcast: PB protocol using 2.
 *	5. Totally-ordered broadcast: get sequence number and then do 2 (PPB).
 *
 * Written by: Frans Kaashoek (11/93).
 */

/*
 * TODO:
 * 	1. More pipelining at the senders. To manage flow control with multiple
 *	senders, the protocol allows one outstanding message per sender.
 *	2. Other protocols: floating sequencer and BB protocol.
 */


/********************** R O U T I N G ******************/

static void
init_routtab(int ncpu)
{
    /* Build routing table. Setup a tree. */

    int i, j;
    int child;
    
    for (i = 0; i < MAXMACHINE; i++) {
        for (j = 0; j < BRANCH_FACTOR; j++) {
	    routtab[i].r_branch[j] = -1;
	}
    }
    
    for (i = 0; i < ncpu; i++) {
	routtab[i].r_branch[0] = (i-1) / BRANCH_CHILD;    /* parent */
	for (j = 1; j < BRANCH_FACTOR; j++) {             /* children */
	    child = BRANCH_CHILD * i + j;
	    routtab[i].r_branch[j] = child < ncpu ? child : -1;
	}
    }  

    routtab[0].r_branch[0] = -1;                /* root has no parent */
}

/********************** S E Q U E N C E R ******************/

static void
handle_seqno(int seqno, int seqno_int, int a2, int a3)
    /* Receive next sequence number from sequencer. */
{
    *(int *)seqno_int = seqno;
}

static void
handle_seqno_request(int src, int seqno_int, int a2, int a3)
{
#ifdef FLOW_CONTROL
    if (global_seqno - last_seqno > pan_mcast_slack) {
	delay_seqno(src, seqno_int);
	return;
    }
#endif

    global_seqno++;
    CMAML_reply(src, handle_seqno, global_seqno, seqno_int, 0, 0);   
}

static int
rpc_getseqno(void)
    /* Get a sequence number from the sequencer. */
{
    volatile int seqno = -1;

    CMAML_request(SEQUENCER, handle_seqno_request,
		  pan_sys_pid, (int)&seqno, 0, 0); 
    /*
    CMAML_poll_while_lt(&seqno, 0);
    */
    while (seqno < 0) pan_poll();

    return seqno;
}


/********************** M U L T I C A S T I N G ******************/

static void
tree_broadcast(pan_fragment_p f, int skip)
{
    int i, dst;
    
    for (i = 0; i < BRANCH_FACTOR; i++) {
	dst = routtab[pan_sys_pid].r_branch[i];
	if (dst < 0 || dst == skip) {   /* don't bounce to source */
	    continue;
	}
	do_send("tree_broadcast", dst, f);
    }
}

static void
process_protocol_msg(void)
{
    pan_fragment_p f;
    
    /*
     * Process all messages in the protocol queue. The messages
     * have already been tagged with a sequence number; just
     * forward them.
     */
    while ((f = dequeue_msg(&msg_process)) != 0) {
	if (f->f_src != pan_sys_pid)  {
	    tree_broadcast(f, f->f_src);
	}
	insert_msg(&msg_deliver, f);      /* and deliver it locally */
    }

    /*
     * Deliver multicast messages locally.
     */
    while (msg_deliver.q_first &&
	   msg_deliver.q_first->f_seqno == last_seqno + 1) {

	f = dequeue_msg(&msg_deliver);
	last_seqno = f->f_seqno;

#ifdef FLOW_CONTROL
	if (global_seqno - last_seqno <= pan_mcast_slack) {
	    delay_flush();
	}
#endif
	/*
	 * Now we can put the message fragment in the comm_daemon's queue.
	 */
	assert(f->f_bufent);
	rbuf_append(f->f_bufent);
    }
}

static void
handle_bye_bye(pan_fragment_p f)
{
    assert(0 <= bye_bye_count && bye_bye_count < pan_sys_nr); 
    bye_bye_count++;
}

/******************************************************************/
/*   I N I T I A L I S A T I O N / T E R M I N A T I O N          */
/******************************************************************/

void
pan_sys_comm_start(void)
{
    pan_fragment_p f;
    int i;

#ifdef FLOW_CONTROL
    if (pan_sys_pid == 0) {
	if (pan_mcast_slack == 0) {
	    pan_mcast_slack = pan_sys_nr / 2;
	}
	if (pan_mcast_slack < 10) {
	    pan_mcast_slack = 10;
	}
    }
#endif
    if (pan_sys_nr > MAXMACHINE) {
	CMMD_error("pan_sys_comm_start: too many (%d) machines requested\n",
		   pan_sys_nr);
    }

    for (i = 0; i < MAX_RBUF; i++) {
	f = pan_fragment_create();
	f->f_next = free_frags;
	free_frags = f;
    }

    init_routtab(pan_sys_nr);
#ifdef VERBOSE
    print_routtab(pan_sys_nr);
#endif
    rbuf_init();
    msg_init();
}

void
pan_sys_comm_wakeup(void)
{
    bye_bye_nsap = pan_nsap_create();
    pan_nsap_fragment(bye_bye_nsap, handle_bye_bye, 0, PAN_NSAP_MULTICAST);

    network_daemon = pan_thread_create(comm_daemon, (void *)0,
				       DAEMON_STACK_SIZE,
				       pan_thread_minprio(), 0);
    pan_thread_daemon(network_daemon);
}

void
pan_sys_comm_end(void)
{
    pan_pset_p all;
    pan_msg_p bye_bye_msg; 
    pan_fragment_p bye_bye_frag;
    pan_fragment_p frag, next_frag;

    /*
     * Termination is tricky. We should not terminate before
     * all multicast messages have been flushed from the network.
     * In particular, we must continue forwarding other nodes'
     * multicast messages.
     */
    all = pan_pset_create();
    pan_pset_fill(all);
    bye_bye_msg = pan_msg_create();
    bye_bye_frag = pan_msg_fragment(bye_bye_msg, bye_bye_nsap);
    pan_comm_multicast_fragment(all, bye_bye_frag);
    pan_pset_clear(all);

    while (bye_bye_count < pan_sys_nr) {
	pan_poll();
    }

    /*
     * Now kill the receive daemon.
     */
    pan_small_enqueue(0, 0, 0, 0, -1);  /* insert termination message */
    pan_thread_join(network_daemon);
    pan_thread_clear(network_daemon);

    pan_nsap_clear(bye_bye_nsap);

    for (frag = free_frags; frag; frag = next_frag) {
	next_frag = frag->f_next;
	pan_fragment_clear(frag);
    }

    printf("%d: %d multisends\n", pan_sys_pid, num_multisends);
}

/******************************************************************/
/*                           A P I                                */
/******************************************************************/

void
pan_poll(void)
{
    CMAML_poll();
    process_protocol_msg();
    if (rbuf_head != 0) {
	pan_thread_run_daemon();
    }
}

void
pan_comm_unicast_fragment(int dest, pan_fragment_p f)
{
    f->f_size  = f->size + FRAG_HDR_SIZE + f->nsap->hdr_size;
    f->f_root  = pan_sys_nr;
    f->f_seqno = -1;
    do_send("pan_comm_unicast_fragment", dest, f);
}

void
pan_comm_unicast_small(int dest, pan_nsap_p nsap, void *data)
{
    int i, argv[4];
    char *ap, *dp;

    assert(nsap->type == PAN_NSAP_SMALL);
    assert(nsap->data_len <= sizeof(argv));

    /* Copy data into argv which is 4 words big. Avoid
     * reading beyond the user data.
     */
    ap = (char *)argv;
    dp = (char *)data;
    for (i = 0; i < nsap->data_len; i++) {
	*ap++ = *dp++;
    }
    
    CMAML_request(dest, PAN_SMALL_HANDLER(nsap->mapid),
		  argv[0], argv[1], argv[2], argv[3]);
}

void
pan_comm_multicast_small(pan_pset_p pset, pan_nsap_p nsap, void *data)
{
    CMMD_error("pan_comm_unicast_small: not implemented\n");
}

void
pan_comm_multicast_fragment(pan_pset_p pset, pan_fragment_p f)
{
    int size = f->size + FRAG_HDR_SIZE + f->nsap->hdr_size;
    pan_receive_p rb;

    assert(size > 0 && (unsigned)size < (1 << 31));

    while ((rb = rbuf_alloc(f->nsap)) == 0) {
	pan_poll();
    }
    f->f_bufent = rb;
    f->f_src    = pan_sys_pid;
    f->f_root   = pan_sys_pid;
    f->f_size   = size;
    f->f_next   = 0;

    rb->r_frag = f;

    if (pan_sys_pid == SEQUENCER) {
	f->f_seqno = ++global_seqno;
	tree_broadcast(f, -1);
	insert_msg(&msg_deliver, f);       /* local delivery (ordered) */
    } else {
	f->f_seqno = rpc_getseqno();       /* ask sequencer for a seqno */
	tree_broadcast(f, -1);
	insert_msg(&msg_deliver, f);       /* local delivery (ordered) */ 
    }
    process_protocol_msg();		/* we can be sender and receiver */
}


