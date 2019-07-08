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
#include "continuation.h"
#include "policy.h"
#include "rts_comm.h"
#include "thrpool.h"


#define UNTAGGED 0   /* don't delay replies with this seqno */

typedef struct service service_t, *service_p;
struct service {
    char        *name;     /* service's ASCII name */
    int          no_ops;   /* no. of operations    */
    operation_p *ops;      /* operation table      */
};


/* This header is prepended to all outgoing RPC requests.
 */
struct rpc_req_hdr {
    unsigned long seqno;
    int           opindex;
    int           service;
};


/* This header is prepended to all outgoing mcasts.
 */
struct mcast_hdr {
    int        opindex;
    int        service;
};


struct rpc_cont {
    struct rpc_req_hdr hdr;      /* header popped from message */
    pan_upcall_t       upcall;   /* handle for reply */
    message_p          request;  /* request message itself */
};


struct mcast_cont {
    struct mcast_hdr *hdr;       /* header popped from message */
    message_p        msg;        /* the mcast message itself */
};


/* Yek! needed for interfacing to thr_pool */
typedef struct {
    pan_upcall_t upcall;
    message_p request;
} rpc_job_t, *rpc_job_p;


static int stab_size;
static service_t stab[RC_MAX_TABSIZE];

static unsigned num_requests_delayed;      /* no. of delayed requests       */
static unsigned num_replies_delayed;       /* no. of delayed replies        */
static unsigned long seqno = UNTAGGED + 1; /* last group message received   */
static mutex_t rc_lock;                 /* protects seqno, num...delayed */
static cond_t seqno_advanced;              /* signals change in seqno       */
static cont_queue_t cont_id;               /* tail of continuation list     */

static int mcast_blocked = 0;		   /* must mcast be blocked?        */
static cont_queue_t blocked_mcasts;	   /* queue of blocked mcast msgs   */

#ifdef BLOCKING_UPCALLS
static thrpool_t rpc_pool, mcast_pool;	   /* pools for blocking upcalls    */
#endif

/* NOT STATIC FOR STATISTICS!! */
group_t rts_group;              /* contains all Orca nodes */


/*****************************/
/* Message dispatch routines */
/*****************************/

/*
 * Grab the current sequence number and compare it to the tag on the RPC
 * request; the continuation succeeds if the current seqno >= the tag.
 */
static int rpc_request_cont(void *state, mutex_t *lock)
{
    struct rpc_cont *cont = (struct rpc_cont *)state;
    struct rpc_req_hdr *hdr;
    unsigned long s;
    operation_p operation;

    s = seqno;

    hdr = &cont->hdr;
    if (s < hdr->seqno) {
	return CONT_KEEP;
    }

    num_requests_delayed--;


    /*
     * Upcall routine calls one of the following routines: r_fetch(),
     * r_fetch_obj(), r_move(), r_rpc_invocation().
     */
    operation = stab[hdr->service].ops[hdr->opindex];

    sys_mutex_unlock(lock);
    (*operation)(hdr->opindex, cont->upcall, cont->request);
    sys_mutex_lock(lock);

    /* upcall routine is responsible for clearing the request message */

    return CONT_NEXT;
}



static void rpc_dispatch(pan_upcall_t upcall, message_p request)
{
    struct rpc_req_hdr *hdr;
    struct rpc_cont *rc;
    operation_p operation;

    /* Check if the sequence no. is ok. If it is not, then create
     * a continuation and return.
     */
    hdr = (struct rpc_req_hdr *)sys_message_pop(request,
						sizeof(struct rpc_req_hdr),
						alignof(struct rpc_req_hdr));
    sys_mutex_lock(&rc_lock);

    if (seqno < hdr->seqno){
	rc = (struct rpc_cont *)cont_alloc(&cont_id, sizeof(*rc),
					   rpc_request_cont);
	rc->hdr     = *hdr;
	rc->upcall  = upcall;
	rc->request = request;
	num_requests_delayed++;
#ifdef BLOCKING_UPCALLS
	cont_save(rc, 1);
#else
	cont_save(rc, 0);
#endif
	sys_mutex_unlock(&rc_lock);
	return;
    } 
    sys_mutex_unlock(&rc_lock);
    
    /*
     * Upcall routine calls one of the following routines: r_fetch(),
     * r_fetch_obj(), r_move(), r_rpc_invocation().
     */
    operation = stab[hdr->service].ops[hdr->opindex];
    (*operation)(hdr->opindex, upcall, request);

    /* upcall routine should clear request message */
}


#ifdef BLOCKING_UPCALLS

static void
put_rpc_dispatch(pan_upcall_t upcall, message_p request)
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

#endif


void
rc_mcast_done(void)
{
    /* Note that a new group message has been handled */
    sys_mutex_lock(&rc_lock);
    seqno++;
    if (num_replies_delayed > 0) {
        sys_cond_broadcast(&seqno_advanced);
    }
    if (num_requests_delayed > 0) {
        cont_resume(&cont_id);
    }
    assert(mcast_blocked == 1);
    mcast_blocked = 0;
    if (cont_pending(&blocked_mcasts)) {
        cont_resume(&blocked_mcasts);
    }
    sys_mutex_unlock(&rc_lock);
}


static int mcast_dispatch_cont(void *state, mutex_t *lock)
{
    struct mcast_cont *cont = (struct mcast_cont *)state;
    operation_p operation;
    pan_upcall_t dummy;

    if (mcast_blocked) {
	return CONT_KEEP;
    }

    mcast_blocked = 1;
    operation = stab[cont->hdr->service].ops[cont->hdr->opindex];  

    sys_mutex_unlock( &rc_lock);
    (*operation)(cont->hdr->opindex, dummy, cont->msg);            /* upcall */
    sys_mutex_lock( &rc_lock);

    grp_message_clear(cont->msg);
    return CONT_NEXT;
}


static void mcast_dispatch(message_p msg)
{
    struct mcast_hdr *hdr;
    operation_p operation;
    pan_upcall_t dummy;
    struct mcast_cont *mc;

    hdr = (struct mcast_hdr *)sys_message_pop(msg,
					      sizeof(struct mcast_hdr),
					      alignof(struct mcast_hdr));

    sys_mutex_lock( &rc_lock);
    if (mcast_blocked) {
	mc = (struct mcast_cont *)cont_alloc(&blocked_mcasts, sizeof(*mc),
					     mcast_dispatch_cont);
	mc->hdr = hdr;
	mc->msg = msg;
#ifdef BLOCKING_UPCALLS
	cont_save(mc, 1);
#else
	cont_save(mc, 0);
#endif
        sys_mutex_unlock( &rc_lock);
    } else {
	mcast_blocked = 1;
        sys_mutex_unlock( &rc_lock);

    	operation = stab[hdr->service].ops[hdr->opindex];  

        (*operation)(hdr->opindex, dummy, msg);            /* upcall */

        grp_message_clear(msg);
    }
}

#ifdef BLOCKING_UPCALLS

static void
put_mcast_dispatch(message_p msg)
{
	thrpool_put(&mcast_pool, msg, message_p);
}

static void
get_mcast_dispatch(void *arg)
{
	mcast_dispatch( *(message_p *)arg);
}

#endif

void 
rc_start(void)
{
#ifdef BLOCKING_UPCALLS
    thrpool_init( &mcast_pool, get_mcast_dispatch, sizeof(message_p),
		  1, 1, sys_thread_minprio()+1);
    thrpool_init( &rpc_pool, get_rpc_dispatch, sizeof(rpc_job_t),
		  sys_nr_platforms, 3, sys_thread_minprio()+1);
    pan_rpc_register(put_rpc_dispatch);
#else
    pan_rpc_register(rpc_dispatch);
#endif
    sys_mutex_init(&rc_lock);
    sys_cond_init(&seqno_advanced);
    cont_init(&cont_id, &rc_lock);
    cont_init(&blocked_mcasts, &rc_lock);
}



void 
rc_end(void)
{
    unsigned i;

    pan_group_leave(&rts_group, NULL);
    cont_clear(&cont_id);
    cont_clear(&blocked_mcasts);
    sys_mutex_lock(&rc_lock);
    assert(num_requests_delayed == 0);
    assert(num_replies_delayed == 0);
    sys_mutex_unlock(&rc_lock);
    sys_mutex_clear(&rc_lock);
    sys_cond_clear(&seqno_advanced);
    for (i = 0; i < stab_size; i++) {
	assert(stab[i].name);
	assert(stab[i].ops);
	sys_free(stab[i].name);
	sys_free(stab[i].ops);
    }
}



int 
rc_export(char *name, int no_ops, operation_p *ops)
{
    int handle;
    int j;

    assert(stab_size < RC_MAX_TABSIZE);
    handle = stab_size++;

    /* initialise entry */
    stab[handle].name   = sys_malloc(strlen(name) + 1);
    (void)strcpy(stab[handle].name, name);
    stab[handle].no_ops = no_ops;
    stab[handle].ops    = sys_malloc(no_ops * sizeof(operation_p));
    for (j = 0; j < no_ops; j++) {
	stab[handle].ops[j] = ops[j];
    }

    return handle;
}



void 
rc_sync_platforms(void)
{
    /* Join group and wait until every other platform has done the same */
#ifdef BLOCKING_UPCALLS
    if (!pan_group_join(&rts_group, "RTS group", put_mcast_dispatch, NULL)) {
#else
    if (!pan_group_join(&rts_group, "RTS group", mcast_dispatch, NULL)) {
#endif
	sys_panic("cannot join RTS group");
    }
 
    /* Hack for dedicated sequencer (who leaves the group immediately)
     * Have only the one that forks OrcaMain wait to avoid the situation
     * that the sequencer has left the group before all members have
     * completed their pan_group_await_size() call.
     */
    if (sys_my_pid == 1) {
        pan_group_await_size(&rts_group, sys_nr_platforms);
    }
}



void 
rc_rpc(int dest, int service, int opindex,
       message_p request, message_p *reply)
/*
 * Do an RPC to dest. A header is prepended that contains the service id,
 * the operation index, and the sequence number of the last group message
 * processed by this process. Note that if the reply is also tagged with
 * a sequence number, it may be delayed after it has arrived. If a tagged
 * reply is expected, then one should not issue this calls from within
 * message upcalls that cannot block for a long time.
 */
{
    unsigned long *s;
    struct rpc_req_hdr *hdr;
    assert(dest != sys_my_pid);
    assert(service < stab_size);
    assert(opindex < stab[service].no_ops);

    /* Marshall service parameters and the seqno */
    sys_mutex_lock(&rc_lock);
    hdr = (struct rpc_req_hdr *)sys_message_push(request,
						 sizeof(struct rpc_req_hdr),
						 alignof(struct rpc_req_hdr));
    hdr->opindex = opindex;
    hdr->service = service;
    hdr->seqno   = seqno;
    sys_mutex_unlock(&rc_lock);

    pan_rpc_trans(dest, request, reply);

/*
    Request messages are never reused, so no need to pop the header....
*/

    /* Delay reply if necessary */
    s = (unsigned long *)sys_message_pop(*reply,
					 sizeof(unsigned long),
					 alignof(unsigned long));
    if (*s == UNTAGGED) {
	return;
    }
    sys_mutex_lock(&rc_lock);
    while (seqno < *s) {
	num_replies_delayed++;
	sys_cond_wait(&seqno_advanced, &rc_lock);
	num_replies_delayed--;
    }
    sys_mutex_unlock(&rc_lock);
}



void 
rc_tagged_reply(pan_upcall_t upcall, message_p reply)
{
    unsigned long *s;

    /* Tag RPC reply with sequence no.
     */
    sys_mutex_lock(&rc_lock);
    s = (unsigned long *)sys_message_push(reply,
					 sizeof(unsigned long),
					 alignof(unsigned long));
    *s = seqno;
    sys_mutex_unlock(&rc_lock);

    pan_rpc_reply(upcall, reply);
}



void 
rc_untagged_reply(pan_upcall_t upcall, message_p reply)
{
    unsigned long *s;

    s = (unsigned long *)sys_message_push(reply,
					 sizeof(unsigned long),
					 alignof(unsigned long));
    *s = UNTAGGED;
    pan_rpc_reply(upcall, reply);
}



void 
rc_mcast(int service, int opindex, message_p msg)
{
    struct mcast_hdr *hdr;

    assert(service < stab_size);
    assert(opindex < stab[service].no_ops);

    hdr = (struct mcast_hdr *)sys_message_push(msg,
					       sizeof(struct mcast_hdr),
					       alignof(struct mcast_hdr));
    hdr->opindex = opindex;
    hdr->service = service;

    pan_group_send(&rts_group, msg);

    /* Mcast messages must not be cleared! */
}

