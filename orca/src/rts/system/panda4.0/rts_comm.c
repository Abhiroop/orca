/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/*
 * Author: Raoul Bhoedjang, March 1994 (last modification)
 *
 * File rts_comm.c: lightweight communication layer for Orca RTS
 *
 * This module provides a primitive service abstraction to the RTS
 * and orders RPC requests and replies w.r.t. group messages.
 * To avoid blocking the thread that makes the upcall for an incoming
 * RPC request when it should wait for some multicast messages, we
 * save that requests state explicitly.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "pan_sys.h"
#include "pan_align.h"
#include "pan_rpc.h"
#ifdef USE_BG
#include "pan_bg.h"
#else
#include "pan_group.h"
#endif
#include "continuation.h"
#include "rts_comm.h"
#include "rts_measure.h"
#include "thrpool.h"
#include "rts_globals.h"


#define RC_MAX_TABSIZE 64   /* max. no. of message handlers */

#define UNTAGGED 0          /* don't delay replies with this seqno */
#define FIRST_SEQNO	UNTAGGED + 1

/* This header is prepended to all outgoing RPC requests.
 */
struct rpc_req_hdr {
    unsigned long rc_seqno;
    int           rc_handle;
};


/* This header is prepended to all RPC replies.
 */
struct rpc_reply_hdr {
    unsigned long rc_seqno;
};


/* This header is prepended to all outgoing mcasts.
 */
struct mcast_hdr {
    int rc_handle;
};


struct rpc_cont {
    struct rpc_req_hdr	*hdr;		/* header popped from message */
    int		upcall;			/* handle for reply */
    pan_msg_p	request;		/* request message itself */
    void       *proto;
};


struct mcast_cont {
    struct mcast_hdr   *hdr;		/* header popped from message */
    unsigned long	my_seqno;	/* to order blocked mcasts */
    pan_msg_p		msg;		/* the mcast message itself */
    void	       *proto;
};


/* Yek! needed for interfacing to thr_pool */
typedef struct {
    int		upcall;
    pan_msg_p	request;
    void       *proto;
} rpc_job_t, *rpc_job_p;

typedef struct{
    pan_msg_p msg;
    void     *proto;
} mc_job_t, *mc_job_p;


/*
static rts_timer_p client_timer;
static int cvalid;
static rts_timer_p seqno_req_timer;
static rts_timer_p seqno_reply_timer;
static rts_timer_p seqno_req_check_timer;
static rts_timer_p seqno_reply_check_timer;
static rts_timer_p rpc_upcall_timer;
static rts_timer_p rts_upcall_timer;
static rts_timer_p pop_seqno_timer;
static rts_timer_p rts_timer;
static rts_timer_p reply_timer;
static rts_timer_p rpc_timer;
*/

int	rts_mcast_proto_start;
int	rts_rpc_proto_start;
int	rts_reply_proto_start;
int	rts_mcast_proto_top;
int	rts_rpc_proto_top;
int	rts_reply_proto_top;
#define  rts_rpc_req_hdr(p)	((struct rpc_req_hdr *)((char *)(p) + rts_rpc_proto_start))
#define  rts_rpc_reply_hdr(p)	((struct rpc_reply_hdr *)((char *)(p) + rts_reply_proto_start))
#define  rts_mcast_hdr(p)	((struct mcast_hdr *)((char *)(p) + rts_mcast_proto_start))

static int stab_size;
static oper_p stab[RC_MAX_TABSIZE];

static unsigned num_replies_delayed;       /* no. of delayed replies        */
/*static*/ unsigned long seqno = FIRST_SEQNO;  /* last group message received   */
static pan_cond_p seqno_advanced;          /* signals change in seqno       */
static cont_queue_t rpc_queue;             /* continuation queue */

static cont_queue_t blocked_mcasts;	   /* queue of blocked mcast msgs   */


static thrpool_t rpc_pool, mcast_pool;	   /* pools for blocking upcalls    */



/* NOT STATIC FOR STATISTICS */
int grp_port;				/* Just for now, use 1 port */

static rc_msg_p msg_freelist;
static void *proto_freelist;

rc_msg_p rc_msg_create(void)
{
    rc_msg_p p = msg_freelist;

    if (p) {
	msg_freelist = p->next;
	return p;
    }
    return m_malloc(sizeof(rc_msg_t));
}

void rc_msg_clear(rc_msg_p p)
{
    p->next = msg_freelist;
    msg_freelist = p;
}

void *rc_proto_create()
{
    void *p = proto_freelist;
    static int maxsz;

    if (p) {
	proto_freelist = *(void **) p;
	return p;
    }
    if (! maxsz) maxsz = pan_proto_max_size();
    return pan_proto_create(maxsz);
}


void rc_proto_clear(void *proto)
{
    *(void **) proto = proto_freelist;
    proto_freelist = proto;
}

/*****************************/
/* Message dispatch routines */
/*****************************/

/*
 * Grab the current sequence number and compare it to the tag on the RPC
 * request; the continuation succeeds if the current seqno >= the tag.
 */
static int
rpc_request_cont(void *state)
{
    struct rpc_cont *cont = (struct rpc_cont *)state;
    struct rpc_req_hdr *hdr;
    unsigned long s;
    oper_p operation;

    assert(!rts_trylock());

    s = seqno;

    hdr = cont->hdr;
    assert(hdr->rc_handle < stab_size);
    if (s < hdr->rc_seqno) {
	return CONT_KEEP;
    }

    operation = stab[hdr->rc_handle];
    if ((*operation)(hdr->rc_handle, cont->upcall, cont->request, cont->proto)) {
    	rts_unlock();
    	pan_msg_clear(cont->request);
    	rts_lock();
    }
    return CONT_NEXT;
}



static void
rpc_dispatch(int upcall, pan_msg_p request, void *proto)
{
    struct rpc_req_hdr *hdr;
    struct rpc_cont *rc;
    oper_p operation;
    int    remove_msg;

    rts_lock();

    /* Check if the sequence no. is ok. If it is not, then create
     * a continuation and return.
     */

/*    rts_measure_enter(rpc_upcall_timer); */
/*    rts_measure_enter(pop_seqno_timer); */
/*    rts_measure_enter(seqno_req_check_timer); */


/*    rts_measure_enter(rts_timer); */

    hdr = rts_rpc_req_hdr(proto);	/* sizeof(struct rpc_req_hdr)); */

/*     rts_measure_leave(pop_seqno_timer); */

    /*
     * Before we process an RPC, we must have processed at least
     * all group messages that have been processed by the sender
     * of the RPC. If this is not the case, we delay the RPC by
     * queueing it on a continuation queue.
     */
    assert(hdr->rc_handle < stab_size);
    if (seqno < hdr->rc_seqno){
	rc = cont_alloc(&rpc_queue, sizeof(*rc), rpc_request_cont);
	rc->hdr     = hdr;
	rc->upcall  = upcall;
	rc->proto   = proto;
	rc->request = request;
	cont_save(rc, use_threads);
/*	rts_measure_leave(seqno_req_check_timer); */
	rts_unlock();
	return;
    }
/*    rts_measure_leave(seqno_req_check_timer); */

    /*
     * Upcall routine calls one of the following routines: r_fetch(),
     * r_fetch_obj(), r_move(), r_rpc_invocation().
     */
    operation = stab[hdr->rc_handle];

    remove_msg = (*operation)(hdr->rc_handle, upcall, request, proto);

    rts_unlock();
    if (remove_msg) pan_msg_clear(request);
}


static void
put_rpc_dispatch(int upcall, pan_msg_p request, void *proto)
{
	rpc_job_t job;

	job.upcall = upcall;
	job.request = request;
	job.proto = proto;
	thrpool_put(&rpc_pool, job, rpc_job_t);
}


static void
get_rpc_dispatch(void *arg)
{
	rpc_job_p job = (rpc_job_p)arg;
	rpc_dispatch( job->upcall, job->request, job->proto);
}


void
rc_mcast_done(void)
{
    /* Avoid running out of stack space when lots of group messages are
     * enqueued. The handling of each message implies a (recursive) call
     * to rc_mcast_done(). Unbounded recursion is prevented by the
     * 'mcast_recursive' flag. Now all pending group messages will be
     * served by one cont_resume() instance.
     */
    static int mcast_recursive = 0;

    /* Note that a new group message has been handled */
    assert(!rts_trylock());

    seqno++;
    if (num_replies_delayed > 0) {
	pan_cond_broadcast(seqno_advanced);
    }
    if (cont_pending(&rpc_queue)) {
	cont_resume(&rpc_queue);
    }
    if (!mcast_recursive && cont_pending(&blocked_mcasts)) {
	mcast_recursive = 1;
	cont_resume(&blocked_mcasts);
	mcast_recursive = 0;
    }
}


static int
mcast_dispatch_cont(void *state)
{
    struct mcast_cont *cont = state;
    oper_p operation;
    int dummy = 0;

    assert(!rts_trylock());

    if (cont->my_seqno > seqno) {
	return CONT_KEEP;
    }

    assert(cont->hdr->rc_handle < stab_size);
    operation = stab[cont->hdr->rc_handle];
    						/* upcall */
    if ((*operation)(cont->hdr->rc_handle, dummy, cont->msg, cont->proto)) {
    	rts_unlock();
    	pan_msg_clear(cont->msg);
    	rts_lock();
    }

    return CONT_NEXT;
}

static void
mcast_dispatch(pan_msg_p msg, void *proto)
{
    struct mcast_hdr *hdr;
    oper_p operation;
    int dummy = -1;
    struct mcast_cont *mc;
    static unsigned long order = FIRST_SEQNO;	/* hand out sequence numbers */
    unsigned long my_seqno;

    rts_lock();

    hdr = rts_mcast_hdr(proto);	/* sizeof(struct mcast_hdr)); */
    assert(hdr->rc_handle < stab_size);

    my_seqno = order++;
    assert( my_seqno >= seqno);
 
    /*
     * This group message should not be processed before
     * its predecessor has finished. If this is not the
     * case, then we queue this message.
     */
    if (my_seqno > seqno) {
	mc = cont_alloc(&blocked_mcasts, sizeof(*mc), mcast_dispatch_cont);
	mc->hdr = hdr;
	mc->my_seqno = my_seqno;
	mc->msg = msg;
	mc->proto = proto;
	cont_save(mc, use_threads);

	rts_unlock();
    } else {
	int remove_msg;
	operation = stab[hdr->rc_handle];
	assert(operation != NULL);
	remove_msg = (*operation)(hdr->rc_handle, dummy, msg, proto); /* upcall */
	rts_unlock();
	if (remove_msg) pan_msg_clear(msg);
    }
}


static void
put_mcast_dispatch(pan_msg_p msg, void *proto)
{
    mc_job_t job;

    job.msg = msg;
    job.proto = proto;
    thrpool_put(&mcast_pool, job, mc_job_t);
}

static void
get_mcast_dispatch(void *arg)
{
    mc_job_p job = (mc_job_p)arg;
    mcast_dispatch(job->msg, job->proto);
}


void
rc_start(void)
{
    if (use_threads) {
	thrpool_init( &mcast_pool, get_mcast_dispatch, sizeof(mc_job_t),
		     1, 1, pan_thread_minprio()+1);
	thrpool_init( &rpc_pool, get_rpc_dispatch, sizeof(rpc_job_t),
		     rts_nr_platforms, 3, pan_thread_minprio()+1);
	pan_rpc_register(put_rpc_dispatch);
#ifdef USE_BG
	grp_port = pan_bg_register_port(put_mcast_dispatch);
#else
	grp_port = pan_group_register_port(put_mcast_dispatch);
#endif
    } else {
	pan_rpc_register(rpc_dispatch);
#ifdef USE_BG
	grp_port = pan_bg_register_port(mcast_dispatch);
#else
	grp_port = pan_group_register_port(mcast_dispatch);
#endif
    }

    rts_mcast_proto_start = align_to(pan_group_proto_offset(), struct mcast_hdr);
    rts_mcast_proto_top = rts_mcast_proto_start + sizeof(struct mcast_hdr);
    rts_rpc_proto_start = align_to(pan_rpc_proto_offset(), struct rpc_req_hdr);
    rts_rpc_proto_top = rts_rpc_proto_start + sizeof(struct rpc_req_hdr);
    rts_reply_proto_start = align_to(pan_rpc_proto_offset(), struct rpc_reply_hdr);
    rts_reply_proto_top = rts_reply_proto_start + sizeof(struct rpc_reply_hdr);

    seqno_advanced = rts_cond_create();
    cont_init(&rpc_queue);
    cont_init(&blocked_mcasts);

/*
    client_timer = rts_measure_create("client overhead");
    rts_timer = rts_measure_create("RTS overhead");
    seqno_reply_timer = rts_measure_create("add reply seqno");
    seqno_req_check_timer = rts_measure_create("check request seqno");
    seqno_req_timer = rts_measure_create("add request seqno");
    pop_seqno_timer = rts_measure_create("pop seqno");
    seqno_reply_check_timer = rts_measure_create("check reply seqno");
    rts_upcall_timer = rts_measure_create("rpc upcall");
    rts_upcall_timer = rts_measure_create("rts upcall");
    reply_timer = rts_measure_create("send reply");
    rpc_timer = rts_measure_create("Panda RPC");
*/
}



void
rc_end(void)
{
    cont_clear(&rpc_queue);
    cont_clear(&blocked_mcasts);
    assert(num_replies_delayed == 0);
    rts_cond_clear(seqno_advanced);
}



int
rc_export(oper_p handler)
{
    int handle;

    assert(stab_size < RC_MAX_TABSIZE);
    handle = stab_size++;
    stab[handle] = handler;
    return handle;
}



void
rc_sync_platforms(void)
{
#ifdef NEVER
    /* Hack for dedicated sequencer (who leaves the group immediately)
     * Have only the one that forks OrcaMain wait to avoid the situation
     * that the sequencer has left the group before all members have
     * completed their pan_group_await_size() call.
     */
#endif
}



void
rc_rpc(int dest, int handle, pan_iovec_p iov, int iov_size,
       void *proto, int proto_size,
       pan_msg_p *reply, void **reply_proto)
/*
 * Do an RPC to dest. A header is prepended that contains the handle
 * and the sequence number of the last group message
 * processed by this process. Note that if the reply is also tagged with
 * a sequence number, it may be delayed after it has arrived. If a tagged
 * reply is expected, then one should not issue this calls from within
 * message upcalls that cannot block for a long time.
 */
{
    struct rpc_reply_hdr *s;
    struct rpc_req_hdr *hdr;

    assert(dest != rts_my_pid);
    assert(handle < stab_size);
    assert(!rts_trylock());

    /* Marshall service parameters and the seqno */

/*     rts_measure_enter(seqno_req_timer); */

    hdr = rts_rpc_req_hdr(proto);	/* sizeof(struct rpc_req_hdr) */
    hdr->rc_handle = handle;
    hdr->rc_seqno  = seqno;

/*
   rts_measure_leave(seqno_req_timer);
   rts_measure_enter(rpc_timer);
   if (cvalid) {cvalid = 0; rts_measure_leave(client_timer); }
*/

    rts_unlock();
    pan_rpc_trans(dest, iov, iov_size, proto, proto_size, reply, reply_proto);
    rts_lock();

/*
   cvalid = 1;
   rts_measure_enter(client_timer);
   rts_measure_leave(rpc_timer);
*/

/*
    Request messages are never reused, so no need to pop the header....
*/

    /*
     * Delay reply if necessary
     */
/*    rts_measure_enter(seqno_reply_check_timer); */
    s = rts_rpc_reply_hdr(*reply_proto);	/* sizeof(unsigned long) */

    if (s->rc_seqno == UNTAGGED) {
/* 	rts_measure_leave(seqno_reply_check_timer); */
	return;
    }
    while (seqno < s->rc_seqno) {
	num_replies_delayed++;
	pan_cond_wait(seqno_advanced);
	num_replies_delayed--;
    }
/*    rts_measure_leave(seqno_reply_check_timer); */
}



void
rc_tagged_reply(int upcall, rc_msg_p msg)
{
    struct rpc_reply_hdr *s;

    /* Tag RPC reply with sequence no.
     */
    assert(!rts_trylock());

    s = rts_rpc_reply_hdr(msg->proto);	/* sizeof(unsigned long)); */
    s->rc_seqno = seqno;

    rts_unlock();			/* not sure about this */
    pan_rpc_reply(upcall, msg->iov, msg->iov_size,
		  msg->proto, msg->proto_size, msg->clearfunc, msg);
    rts_lock();

/*    rts_measure_leave(reply_timer); */
}


void
rc_untagged_reply(int upcall, rc_msg_p msg)
{
    struct rpc_reply_hdr  *s;

    assert(!rts_trylock());

    s = rts_rpc_reply_hdr(msg->proto);	/* sizeof(unsigned long)); */
    s->rc_seqno = UNTAGGED;
    rts_unlock();                  /* not sure about this */
    pan_rpc_reply(upcall, msg->iov, msg->iov_size,
		  msg->proto, msg->proto_size, msg->clearfunc, msg);
    rts_lock();
}



void
rc_mcast(int handle, pan_iovec_p iov, int iov_size, void *proto, int proto_size)
{
    struct mcast_hdr *hdr;

    assert(handle < stab_size);

    hdr = rts_mcast_hdr(proto);	/* sizeof(struct mcast_hdr)); */
    hdr->rc_handle = handle;

    rts_unlock();
#ifdef USE_BG
    {
	int ticket;
	ticket = pan_bg_send(grp_port, msg, len);
	pan_bg_async_send(ticket);
    }
#else
    pan_group_send(grp_port, iov, iov_size, proto, proto_size);
#endif
    rts_lock();

    /* Mcast messages must not be cleared! */
}
