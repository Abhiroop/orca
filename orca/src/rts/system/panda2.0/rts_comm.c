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


/* This header is prepended to all outgoing mcasts.
 */
struct mcast_hdr {
    int rc_handle;
};


struct rpc_cont {
    struct rpc_req_hdr hdr;      /* header popped from message */
    pan_upcall_p       upcall;   /* handle for reply */
    pan_msg_p          request;  /* request message itself */
};


struct mcast_cont {
    struct mcast_hdr *hdr;       /* header popped from message */
    unsigned long     my_seqno;	 /* to order blocked mcasts */
    pan_msg_p         msg;       /* the mcast message itself */
};


/* Yek! needed for interfacing to thr_pool */
typedef struct {
    pan_upcall_p upcall;
    pan_msg_p    request;
} rpc_job_t, *rpc_job_p;


/*
static int cvalid;
static rts_timer_p client_timer; 
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

#ifdef TR_HACK
static pan_msg_p hack_msg1, hack_msg2;
#endif


static int stab_size;
static oper_p stab[RC_MAX_TABSIZE];

static unsigned num_replies_delayed;       /* no. of delayed replies        */
static unsigned long seqno = FIRST_SEQNO;  /* last group message received   */
static pan_mutex_p rc_lock;                /* protects seqno, num...delayed */
static pan_cond_p seqno_advanced;          /* signals change in seqno       */
static cont_queue_t rpc_queue;             /* continuation queue */

static cont_queue_t blocked_mcasts;	   /* queue of blocked mcast msgs   */


static thrpool_t rpc_pool, mcast_pool;	   /* pools for blocking upcalls    */



/* NOT STATIC FOR STATISTICS */
#ifdef USE_BG
int bg_port;
#else
pan_group_p rts_group;                     /* contains all Orca nodes */
#endif

/*****************************/
/* Message dispatch routines */
/*****************************/

/*
 * Grab the current sequence number and compare it to the tag on the RPC
 * request; the continuation succeeds if the current seqno >= the tag.
 */
static int
rpc_request_cont(void *state, pan_mutex_p lock)
{
    struct rpc_cont *cont = (struct rpc_cont *)state;
    struct rpc_req_hdr *hdr;
    unsigned long s;
    oper_p operation;

    s = seqno;

    hdr = &cont->hdr;
    if (s < hdr->rc_seqno) {
	return CONT_KEEP;
    }

    pan_mutex_unlock(lock);
    operation = stab[hdr->rc_handle];
    (*operation)(hdr->rc_handle, cont->upcall, cont->request);
    pan_mutex_lock(lock);

    /* upcall routine is responsible for clearing the request message */
    return CONT_NEXT;
}



static void
rpc_dispatch(pan_upcall_p upcall, pan_msg_p request)
{
    struct rpc_req_hdr *hdr;
    struct rpc_cont *rc;
    oper_p operation;

#ifdef TR_HACK
    if (hack_msg1){
	pan_msg_p tmp = hack_msg1;

	pan_rpc_reply(upcall, hack_msg1);

	hack_msg1 = pan_msg_create();
	if (tmp == hack_msg1){
	    printf("Fast ack\n");
	}

 	pan_msg_copy(hack_msg2, hack_msg1);
	pan_msg_clear(request);

	return;
    }
#endif

    /* Check if the sequence no. is ok. If it is not, then create
     * a continuation and return.
     */

/*    rts_measure_enter(rpc_upcall_timer); */
/*    rts_measure_enter(pop_seqno_timer); */
/*    rts_measure_enter(seqno_req_check_timer); */


/*    rts_measure_enter(rts_timer); */

    hdr = (struct rpc_req_hdr *)pan_msg_pop(request,
					    sizeof(struct rpc_req_hdr),
					    alignof(struct rpc_req_hdr));

/*     rts_measure_leave(pop_seqno_timer); */

    pan_mutex_lock(rc_lock);

    if (seqno < hdr->rc_seqno){
	rc = (struct rpc_cont *)cont_alloc(&rpc_queue, sizeof(*rc),
					   rpc_request_cont);
	rc->hdr     = *hdr;
	rc->upcall  = upcall;
	rc->request = request;
	cont_save(rc, use_threads);
	pan_mutex_unlock(rc_lock);
/*	rts_measure_leave(seqno_req_check_timer); */
	return;
    } 
    pan_mutex_unlock(rc_lock);
/*    rts_measure_leave(seqno_req_check_timer); */
    
    /*
     * Upcall routine calls one of the following routines: r_fetch(),
     * r_fetch_obj(), r_move(), r_rpc_invocation().
     */
    operation = stab[hdr->rc_handle];

/*     rts_measure_enter(rts_upcall_timer); */
    (*operation)(hdr->rc_handle, upcall, request);
/*     rts_measure_leave(rpc_upcall_timer); */

    /* upcall routine should clear request message */
}


void
put_rpc_dispatch(pan_upcall_p upcall, pan_msg_p request)
{
	rpc_job_t job;

	job.upcall = upcall;
	job.request = request;
	thrpool_put(&rpc_pool, job, rpc_job_t);
}


static void
get_rpc_dispatch(void *arg)
{
	rpc_job_p job = (rpc_job_p)arg;
	rpc_dispatch( job->upcall, job->request);
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
    pan_mutex_lock(rc_lock);
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
    pan_mutex_unlock(rc_lock);
}


static int
mcast_dispatch_cont(void *state, pan_mutex_p lock)
{
    struct mcast_cont *cont = (struct mcast_cont *)state;
    oper_p operation;
    pan_upcall_p dummy = NULL;

    if (cont->my_seqno > seqno) {
	return CONT_KEEP;
    }

    pan_mutex_unlock(rc_lock);
    operation = stab[cont->hdr->rc_handle];
    (*operation)(cont->hdr->rc_handle, dummy, cont->msg);      /* upcall */
    pan_mutex_lock(rc_lock);

    pan_msg_clear(cont->msg);
    return CONT_NEXT;
}

static void
mcast_dispatch(pan_msg_p msg)
{
    struct mcast_hdr *hdr;
    oper_p operation;
    pan_upcall_p dummy = NULL;
    struct mcast_cont *mc;
    static unsigned long order = FIRST_SEQNO;	/* hand out sequence numbers */
    unsigned long my_seqno;

    hdr = (struct mcast_hdr *)pan_msg_pop(msg, sizeof(struct mcast_hdr),
					  alignof(struct mcast_hdr));

    pan_mutex_lock(rc_lock);
    my_seqno = order++;
    assert( my_seqno >= seqno);
    if (my_seqno > seqno) {
	mc = (struct mcast_cont *)cont_alloc(&blocked_mcasts, sizeof(*mc),
					     mcast_dispatch_cont);
	mc->hdr = hdr;
	mc->my_seqno = my_seqno;
	mc->msg = msg;
	cont_save(mc, use_threads);
        pan_mutex_unlock(rc_lock);
    } else {
        pan_mutex_unlock(rc_lock);

    	operation = stab[hdr->rc_handle];  
        (*operation)(hdr->rc_handle, dummy, msg);            /* upcall */

        pan_msg_clear(msg);
    }
}


void
put_mcast_dispatch(pan_msg_p msg)
{
    thrpool_put(&mcast_pool, msg, pan_msg_p);
}

static void
get_mcast_dispatch(void *arg)
{
    mcast_dispatch( *(pan_msg_p *)arg);
}


void 
rc_start(void)
{
    if (use_threads) {
	thrpool_init( &mcast_pool, get_mcast_dispatch, sizeof(pan_msg_p),
		     1, 1, pan_thread_minprio()+1);
	thrpool_init( &rpc_pool, get_rpc_dispatch, sizeof(rpc_job_t),
		     rts_nr_platforms, 3, pan_thread_minprio()+1);
	pan_rpc_register(put_rpc_dispatch);
#ifdef USE_BG
	bg_port = pan_bg_register_port(put_mcast_dispatch);
#endif
    } else {
	pan_rpc_register(rpc_dispatch);
#ifdef USE_BG
	bg_port = pan_bg_register_port( mcast_dispatch);
#endif
    }

    rc_lock = pan_mutex_create();
    seqno_advanced = pan_cond_create(rc_lock);
    cont_init(&rpc_queue, rc_lock);
    cont_init(&blocked_mcasts, rc_lock);

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
    char    *grp_stats;

#ifdef USE_BG
    pan_bg_free_port(bg_port);
#else
    pan_group_leave(rts_group);
    if (statistics) {			/* Added statistics print RFHH */
	pan_group_va_get_g_params(rts_group, "PAN_GRP_statistics", &grp_stats,
				  NULL);
	printf(grp_stats);
	pan_free(grp_stats);
    }
    pan_group_clear(rts_group);
#endif
    cont_clear(&rpc_queue);
    cont_clear(&blocked_mcasts);
    pan_mutex_lock(rc_lock);
    assert(num_replies_delayed == 0);
    pan_mutex_unlock(rc_lock);
    pan_cond_clear(seqno_advanced);
    pan_mutex_clear(rc_lock);
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
    /* Join group and wait until every other platform has done the same */
#ifdef USE_BG
    /* XXX: TODO real synchronization */
#else
    if (use_threads) {
	rts_group = pan_group_join("RTS group", put_mcast_dispatch);
    } else {
	rts_group = pan_group_join("RTS group", mcast_dispatch);
    }

    if (!rts_group) {
	fprintf(stderr, "%d) cannot join RTS group\n", rts_my_pid);
	exit(1);
    }

    /* Hack for dedicated sequencer (who leaves the group immediately)
     * Have only the one that forks OrcaMain wait to avoid the situation
     * that the sequencer has left the group before all members have
     * completed their pan_group_await_size() call.
     */
    if (rts_my_pid == 0) {
        pan_group_await_size(rts_group, rts_nr_platforms);
    }
#endif
}



void 
rc_rpc(int dest, int handle, pan_msg_p request, pan_msg_p *reply)
/*
 * Do an RPC to dest. A header is prepended that contains the handle
 * and the sequence number of the last group message
 * processed by this process. Note that if the reply is also tagged with
 * a sequence number, it may be delayed after it has arrived. If a tagged
 * reply is expected, then one should not issue this calls from within
 * message upcalls that cannot block for a long time.
 */
{
    unsigned long *s;
    struct rpc_req_hdr *hdr;

    assert(dest != rts_my_pid);
    assert(handle < stab_size);

    /* Marshall service parameters and the seqno */

/*     rts_measure_enter(seqno_req_timer); */

    pan_mutex_lock(rc_lock);
    hdr = (struct rpc_req_hdr *)pan_msg_push(request,
						 sizeof(struct rpc_req_hdr),
						 alignof(struct rpc_req_hdr));
    hdr->rc_handle = handle;
    hdr->rc_seqno  = seqno;
    pan_mutex_unlock(rc_lock);

/* 
   rts_measure_leave(seqno_req_timer);
   rts_measure_enter(rpc_timer);
   if (cvalid) {cvalid = 0; rts_measure_leave(client_timer); }
*/

    pan_rpc_trans(dest, request, reply);

/*
   cvalid = 1;
   rts_measure_enter(client_timer);
   rts_measure_leave(rpc_timer); 
*/

/*
    Request messages are never reused, so no need to pop the header....
*/

    /* Delay reply if necessary */
/*    rts_measure_enter(seqno_reply_check_timer); */
    s = (unsigned long *)pan_msg_pop(*reply, sizeof(unsigned long),
				     alignof(unsigned long));
    if (*s == UNTAGGED) {
/* 	rts_measure_leave(seqno_reply_check_timer); */
	return;
    }
    pan_mutex_lock(rc_lock);
    while (seqno < *s) {
	num_replies_delayed++;
	pan_cond_wait(seqno_advanced);
	num_replies_delayed--;
    }
    pan_mutex_unlock(rc_lock);
/*    rts_measure_leave(seqno_reply_check_timer); */
}



void 
rc_tagged_reply(pan_upcall_p upcall, pan_msg_p reply)
{
    unsigned long *s;

    /* Tag RPC reply with sequence no.
     */
/*    rts_measure_enter(seqno_reply_timer); */
    pan_mutex_lock(rc_lock);
    s = (unsigned long *)pan_msg_push(reply, sizeof(unsigned long),
				      alignof(unsigned long));
    *s = seqno;
    pan_mutex_unlock(rc_lock);
/*    rts_measure_leave(seqno_reply_timer); */

#ifdef TR_HACK
    printf("BOE\n");
    hack_msg1 = pan_msg_create();
    hack_msg2 = pan_msg_create();
    
    pan_msg_copy(reply, hack_msg1);
    pan_msg_copy(reply, hack_msg2);
#endif

/*    rts_measure_leave(rts_timer); */
/*    rts_measure_enter(reply_timer); */
    pan_rpc_reply(upcall, reply);
/*    rts_measure_leave(reply_timer); */
}



void 
rc_untagged_reply(pan_upcall_p upcall, pan_msg_p reply)
{
    unsigned long *s;

    s = (unsigned long *)pan_msg_push(reply, sizeof(unsigned long),
				      alignof(unsigned long));
    *s = UNTAGGED;
    pan_rpc_reply(upcall, reply);
}



void 
rc_mcast(int handle, pan_msg_p msg)
{
    struct mcast_hdr *hdr;

    assert(handle < stab_size);

    hdr = (struct mcast_hdr *)pan_msg_push(msg, sizeof(struct mcast_hdr),
					   alignof(struct mcast_hdr));
    hdr->rc_handle = handle;

#ifdef USE_BG
    pan_bg_send_async(bg_port, msg);
#else
    pan_group_send(rts_group, msg);
#endif

    /* Mcast messages must not be cleared! */
}

