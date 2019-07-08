/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#include <interface.h>
#include "pan_sys.h"
#include "continuation.h"
#include "fragment.h"
#include "invocation.h"
#include "manager.h"
#include "msg_marshall.h"
#include "obj_tab.h"
#include "proxy.h"
#include "rts_comm.h"
#include "rts_measure.h"
#include "rts_trace.h"
#include "account.h"
#include "rts_globals.h"
#include "inline.h"

#define OP_DEADLOCK (-1)

/*
static rts_timer_p marshall_req_timer;
static rts_timer_p unmarshall_req_timer;
static rts_timer_p marshall_reply_timer;
static rts_timer_p unmarshall_reply_timer;
static rts_timer_p operation_timer;
*/
rts_timer_p compiled_timer;

#ifdef PURE_WRITE

/*
 * An operation is a pure write when it satisfies two conditions:
 * 1. It only takes IN parameters    (OP_PURE_WRITE).
 * 2. It can never block on a guard  (OP_BLOCKING).
 */
int
rts_is_pure_write(tp_dscr *obj_type, int opindex)
{
    op_dscr *op = &(td_operations(obj_type)[opindex]);

    return (op->op_flags & OP_PURE_WRITE) &&
	   !(op->op_flags & OP_BLOCKING);
}

/*
 * rts_flush_pure_writes() blocks its caller when this caller wants to
 * perform an operation on a shared object while there are operations
 * pending on another shared object. If always != 0, then the caller
 * is always blocked, even if an operation on the same object is
 * attempted.
 */
void
rts_flush_pure_writes(process_p self, fragment_p obj, int always)
{
    assert(obj);
    if (always || obj != self->p_object) {
	while(self->p_nwrites > 0) {
	    pan_cond_wait(self->p_flushed);
	}
    }
}

/*
 * start_next_write() attempts to pipeline a new write operation.
 * The attempt fails if this is a write on another object. In this
 * case, we wait until all pending writes have been flushed.
 */
static void
start_next_write(process_p self, fragment_p obj)
{
    if (self->p_nwrites == 0) {
	self->p_object = obj;           /* first in pipeline; register obj */
	self->p_pipelined++;
    } else if (obj != self->p_object) {
	/*
	 * Other object; no pipelining allowed, so flush
	 * pending writes first.
	 */
	while(self->p_nwrites > 0) {
	    pan_cond_wait(self->p_flushed);
	}
    } else {
	self->p_pipelined++;
    }
    self->p_nwrites++;                  /* register this pure write */
}

static void
pure_write_done(process_p proc)
{
    assert(proc->p_nwrites >= 1);

    if (--proc->p_nwrites == 0) {
	proc->p_object = 0;
	pan_cond_signal(proc->p_flushed);
    }
}

#endif

/***********************************/
/* Client-side invocation variants */
/***********************************/

static int unshared_invocation(fragment_p, tp_dscr *, int, void **,
			       int *op_flags);
static int replicated_invocation(fragment_p, tp_dscr *, int, void **,
				 int *op_flags);
static int owner_invocation(fragment_p, tp_dscr *, int, void **,
			    int *op_flags);
static int remote_invocation(fragment_p, tp_dscr *, int, void **,
			     int *op_flags);


INLINE int
op_read_only(op_dscr *op)
{
    return op->op_write_alts == 0;
}

/***********************************/
/* Server-side invocation handlers */
/***********************************/

static int mcast_handle;
static int rpc_handle;


/********** Hack by Koen **************/

struct reply_cont {
    int upcall;
    void *reply;
    int size;
    int len;
};

static int
send_result_cont(void *state)
{
    struct reply_cont *rc = state;
 
    rc_tagged_reply(rc->upcall, rc->reply, rc->len);
    return CONT_NEXT;
}

INLINE void
send_result(int upcall, void *reply, int size, int success, 
	    op_dscr *op, void **argv)
{
    struct reply_cont *rc;
    int len = 0;

/*    rts_measure_enter(marshall_reply_timer); */
    mm_pack_op_result(&reply, &size, &len, success, op, argv);
    assert(reply != NULL);
/*    rts_measure_leave(marshall_reply_timer); */

    if ( success && !cont_pending(&cont_immediate_queue)) {
	rc_tagged_reply(upcall, reply, len);
    } else {
	/* Grrr, watch out for starvation in case where a server becomes
	 * overloaded with repeatedly failing rpc requests. Have the
	 * failures send back by a thread running at low priority.
	 *
	 * Also, delay successful rpc requests if the low-priority
	 * has work to do. If not, some important msg may not be transmitted
	 * because the system keeps handling RPC requests. (Awari again:
	 * threads keep on polling a buffer object on CPU 0, who cannot
	 * make progress => program does not terminate)
	 */
	rc = cont_alloc(&cont_immediate_queue, sizeof(*rc), send_result_cont);
	rc->upcall = upcall;
	rc->reply = reply;
	rc->size = size;
	rc->len = len;
	cont_save(rc, 0);
    }
}


/**************************************/
/*      Continuation functions        */
/**************************************/

/* All continuation functions assume that the object that they operate on 
 * has already been locked!!
 */

static int
cont_local_invocation(void *state)
{
    invocation_p i = *(invocation_p *)state;
    int modified;
    op_dscr *op;
    fragment_p frag;
    void **argv;
    int upcall;
    f_status_t status;
    source_t src;
    int *op_flags;
    int opflags = 0;

#ifdef TRACING
    /* No "op_cont" event is generated here. It is generated just after the
     * wakeup of the thread that blocks on the guard.
     */
#endif
    i_get_info_macro(i, op, frag, argv, status, upcall, op_flags);
    if (! op_flags) op_flags = &opflags;
    if (f_get_status(frag) != status) {             /* status has changed */
	i_wakeup(i, I_FAILED);
	return CONT_NEXT;
    }
    src = (op_read_only(op) ? AC_LOCAL_FAST : AC_LOCAL_BC);
    if (f_write_read(frag, op_flags, op, argv, &modified, src)) {
	i_wakeup(i, I_COMPLETED);
	return (modified ? CONT_RESTART : CONT_NEXT);
    }

    return CONT_KEEP;
}


static int
cont_rpc_invocation(void *state)
{
    invocation_p i = (invocation_p)state;
    int modified, success;
    op_dscr *op;
    fragment_p frag;
    void **argv;
    f_status_t status;
    int upcall;
    source_t src;
#ifdef TRACING
    op_cont_info continf;
#endif
    int *op_flags;
    int opflags = 0;

    i_get_info_macro(i, op, frag, argv, status, upcall, op_flags);
    if (! op_flags) op_flags = &opflags;

#ifdef TRACING          /* Added RAFB */
    continf.op    = op->op_index;
    continf.cpu   = frag->fr_home;
    continf.obj   = frag->fr_home_obj;
    continf.state = state;
#endif

    if (f_get_status(frag) != status) {              /* status has changed */
	send_result(upcall, NULL, 0, 0, op, argv); 
	i_clear(i);
	trc_event(cont_done, &continf);
	return CONT_NEXT;
    }
    src = (op_read_only(op) ? AC_REMOTE : AC_REMOTE_BC);
    if (f_write_read(frag, op_flags, op, argv, &modified, src)) {
#ifdef ACCOUNTING
	success = 1 + (modified ? AC_WRITE : AC_READ);
#else
	success = 1;
#endif
	send_result(upcall, NULL, 0, success, op, argv); 
	i_clear(i);
	trc_event(cont_done, &continf);
	return (modified ? CONT_RESTART : CONT_NEXT);
    }
    return CONT_KEEP;
}


static int
cont_mcast_invocation(void *state)
{
    invocation_p i = (invocation_p)state;
    int modified;
    op_dscr *op;
    fragment_p frag;
    void **argv;
    f_status_t status;
    int upcall;
    int *op_flags;
    int opflags = 0;
#ifdef TRACING
    op_cont_info continf;
#endif

    i_get_info_macro(i, op, frag, argv, status, upcall, op_flags);
    if (! op_flags) op_flags = &opflags;

#ifdef TRACING          /* Added RAFB */
    continf.op    = op->op_index;
    continf.cpu   = frag->fr_home;
    continf.obj   = frag->fr_home_obj;
    continf.state = state;
#endif

    if (f_get_status(frag) != status) {
	mm_free_op_args(op, argv);
	i_clear(i);
	trc_event(cont_done, &continf);
	return CONT_NEXT;
    }
    if (f_write_read(frag, op_flags, op, argv, &modified, AC_REMOTE_BC)) {
	mm_free_op_args(op, argv);
	i_clear(i);
	trc_event(cont_done, &continf);
	return (modified ? CONT_RESTART : CONT_NEXT);
    }
    return CONT_KEEP;
}


/**************************************/
/* remote invocation service routines */
/**************************************/

static void 
r_rpc_invocation(int _op, int upcall, void *request, int size, int len)
{
    cont_queue_t *cont_queue;
    int modified, success = 0;
    op_hdr_t *hdr;
    fragment_p f;
    void **argv;
    op_dscr *op;
    man_piggy_p piggy_info;
    int opflags = 0;
    int *op_flags = &opflags;
#ifdef PURE_WRITE
    int pure_write;
#endif

/*    rts_measure_enter(unmarshall_req_timer); */

#ifdef PURE_WRITE
    hdr = mm_unpack_op_call(request, &len, &f, &op, &argv, &piggy_info,
			    &pure_write);
#else
    hdr = mm_unpack_op_call(request, &len, &f, &op, &argv, &piggy_info);
#endif

    if (f) {
	f_status_t status;
#ifdef TRACING
	op_inv_info opinf;
	op_cont_info continf;
	opinf.op  = hdr->oh_opindex;		/* changed RFHH */
	opinf.cpu = f->fr_home;
	opinf.obj = f->fr_home_obj;
	trc_event(op_rpc, &opinf);
#endif

	man_process_piggy_info(piggy_info, f, hdr->oh_src);
/*        rts_measure_leave(unmarshall_req_timer); */
	
	cont_queue = f_get_queue(f);
	status     = f_get_status(f);
/*  	  rts_measure_enter(operation_timer); */
	if (status != f_owner) { /* not owner; data no longer present */
	    success = 0;
	} else if (f_read_write(f, op_flags, op, argv, &modified, AC_REMOTE)) {
#ifdef ACCOUNTING
	    success = 1 + (modified ? AC_WRITE : AC_READ);
#else
	    success = 1;
#endif
	    if (modified && cont_pending(cont_queue)) {
		cont_resume(cont_queue);
	    }
	    man_op_finished(f);
/*	    rts_measure_leave(operation_timer); */
	} else if (f->fr_total_refs == 1) {
	    success = OP_DEADLOCK;
	} else {
	    invocation_p inv = cont_alloc(cont_queue, sizeof(*inv),
							cont_rpc_invocation);
	    i_init(inv, f, status, op, upcall, argv, (int *) 0);
#ifdef TRACING
	    continf.op    = hdr->oh_opindex;
	    continf.cpu   = f->fr_home;
	    continf.obj   = f->fr_home_obj;
	    continf.state = inv;
	    trc_event(cont_create, &continf);
#endif
	    cont_save(inv, use_threads);
	    pan_free(request);
	    return;
	}
    }    

    /* trick: reuse request message to send reply */
    send_result(upcall, request, size, success, op, argv); 
}


/*
 * r_mcast_invocation: handles incoming multicast operation messages.
 * Algorithm:
 * Case 1: (Operation initiated by this platform)
 *      - fetch operation info from the record (inv) that was created
 *        by the local thread that initiated the operation
 *      - lock the object
 *      - check if the recorded status equals the status in the object
 *      - try to perform an operation; if a write succeeded, then resume
 *        all pending continuations; otherwise queue a continuation.
 *      - wake up the thread that initiated the operation
 *
 * Case 2: (Operation message multicast by another platform)
 *      - lookup the object in the object table
 *      - lock the object
 *      - check if the object is still replicated
 *      - unmarshall the operation arguments
 *      - try to perform an operation; if a write succeeded, then resume
 *        all pending continuations; otherwise queue a continuation.
 *     -  clean up the unmarshalled arguments when the operation
 *        succeeded
 */
static void 
r_mcast_invocation(int _op, int dummy, void *request, int size, int len)
{
    cont_queue_t *cont_queue;
    fragment_p f;
    f_status_t status;
    int modified;
    op_dscr *op;
    invocation_p inv, *p;
    op_hdr_t *hdr;
    void **argv;
    man_piggy_p piggy_info;
#ifdef TRACING
    op_inv_info opinf;
    op_cont_info continf;
#endif
    int opflags = 0;
    int *op_flags = &opflags;
 
#ifdef PURE_WRITE
    process_p proc;
    int pure_write;

    hdr = mm_unpack_op_call(request, &len, &f, &op, &argv, &piggy_info,
			    &pure_write);
#else
    hdr = mm_unpack_op_call(request, &len, &f, &op, &argv, &piggy_info);
#endif

    if (hdr->oh_src == rts_my_pid) {      /* Operation initiated locally */
	int upcall;
 
#ifdef PURE_WRITE	
	Raoul: should look at what to do with new evaluation scheme.
        if (pure_write) {
            proc = (process_p)hdr->oh_ptr;
            op->op_index = hdr->oh_opindex;
            op->obj_type = f->fr_type;
            status = f_get_status(f);  /* hack... */
        } else {
            /* args were not unmarshalled; fetch them
             * through local record
             */
	    inv = (invocation_p)hdr->oh_ptr;
	    i_get_info_macro(inv, op, f, argv, status, upcall, op_flags);
	    if (! op_flags) op_flags = &opflags;
	}
#else
	inv = (invocation_p)hdr->oh_ptr;
	i_get_info_macro(inv, op, f, argv, status, upcall, op_flags);
	if (! op_flags) op_flags = &opflags;
#endif

#ifdef TRACING
	opinf.op  = op->op_index;		/* changed RFHH */
	opinf.cpu = f->fr_home;
	opinf.obj = f->fr_home_obj;
	trc_event(op_mcast, &opinf);
#endif
	cont_queue = f_get_queue(f);
	if (f_get_status(f) != status) {
	    i_wakeup(inv, I_FAILED);		/* wakeup invoking thread */
	} else if (f_write_read(f, op_flags, op, argv, &modified, AC_LOCAL)) {
#ifdef PURE_WRITE
	Raoul: should look at what to do with new evaluation scheme.
            if (pure_write) {
                pure_write_done(proc);
                mm_free_op_args(op, argv);
            } else {
                i_wakeup(inv, I_COMPLETED);
            }
#else
	    i_wakeup(inv, I_COMPLETED);		/* wakeup invoking thread */
#endif
	    if (cont_pending(cont_queue)) {
		cont_resume(cont_queue);	/* retry pending ops */
	    }
	} else if (f->fr_total_refs == 1) {	/* object not shared */
	    m_trap(LOCAL_DEADLOCK, (char *) 0, 0);
	} else {
#ifdef PURE_WRITE
	    assert(!pure_write);
#endif
	    p = cont_alloc(cont_queue, sizeof(*p), cont_local_invocation);
	    *p = inv;
	    cont_save(p, 0);
#ifdef TRACING
	    continf.op    = op->op_index;
	    continf.cpu   = f->fr_home;
	    continf.obj   = f->fr_home_obj;
	    continf.state = p;
	    trc_event(cont_create, &continf);
#endif
	}

	/* Do not reconsider earlier obj. distribution decision if the
	 * write on a replicated object is issued locally for two reasons:
	 * 1) The man_take_decision routine has not been notified yet of this
	 *    operation, so it occasionally takes faulty decisions (max_access
	 *    is out of date).
	 * 2) The manager is not aware of remote reads, which leads to
	 *    ping-pong behaviour: replicate-exclusive-replicate-exclusive.
	 *    Take the chance that the object should stay replicated, and
	 *    that switching to exclusive will be initiated by a remote write.
	 */
	/* Hmm, above reasoning fails for MoCa, where an object should be
	 * changed from replicated to local beacuse at a certain moment the
	 * local Orca process is the only process accessing the object.
	 * Note that we now delute man_take_decision, so ping-pong behaviour
	 * is less likely to occur.
	 */
	man_op_finished(f);
	rc_mcast_done();
	return;
    }

    if (!f) {              /* Obj. fragment present? */
	rc_mcast_done();
	return;
    }

    man_process_piggy_info(piggy_info, f, hdr->oh_src);
    status = f_get_status(f);
    if (!(status == f_replicated || status == f_manager)) {
	rc_mcast_done();
	return;
    }
 
#ifdef TRACING
    opinf.op = hdr->oh_opindex;		/* changed RFHH */
    opinf.cpu = f->fr_home;
    opinf.obj = f->fr_home_obj;
    trc_event( op_mcast, &opinf);
#endif
 
    cont_queue = f_get_queue(f);
    if (f_write_read(f, op_flags, op, argv, &modified, AC_REMOTE)) {
	if (modified && cont_pending(cont_queue)) {
	    cont_resume(cont_queue);
	}
	man_op_finished(f);
	mm_free_op_args(op, argv);
    } else {
	inv = (invocation_p)cont_alloc(cont_queue, sizeof(*inv),
				       cont_mcast_invocation);
	if (op_flags == &opflags) op_flags = 0;
	i_init(inv, f, status, op, dummy, argv, op_flags);
	cont_save(inv, 0);
#ifdef TRACING
	continf.op    = hdr->oh_opindex;
	continf.cpu   = f->fr_home;
	continf.obj   = f->fr_home_obj;
	continf.state = inv;
	trc_event(cont_create, &continf);
#endif
	/* Do not clean up args until invocation completes! */
    }
 
    rc_mcast_done();
}


/***********************/
/* Invocation variants */
/***********************/

/* The following functions all handle a particular invocation variant.
 * The choice of the variant depends on the fragment's status. During
 * the intervals that a fragment is not locked, its status may change.
 * Therefore we must recheck fragment status after each such interval.
 */

INLINE int
unshared_invocation(fragment_p f, tp_dscr *obj_type,
		    int opindex, void **argv, int *op_flags)
{
    int modified, completed;
    op_dscr *op = &(td_operations(obj_type)[opindex]);

#ifdef PURE_WRITE
    /*
     * An unshared object is like a process-local variable. It can
     * neither be read nor written by other processes. Therefore, we
     * need never stall an operation on an unshared object, even if
     * pure writes are currently in progress. (The object may become
     * shared later; to avoid trouble in that case, DoFork always
     * waits until all pending writes have completed.) 
     */
#endif
    completed = f_read_write(f, op_flags, op, argv, &modified,
			     AC_LOCAL);
    if (! (*op_flags & NESTED)) {  /* top-level? */
	if (!completed && f->fr_total_refs == 1) {
	    m_trap(LOCAL_DEADLOCK, (char *) 0, 0);
	}	    
    }
    return 1;
}



INLINE int
replicated_invocation(fragment_p f, tp_dscr *obj_type,
		      int opindex, void **argv, int *op_flags)
{
    op_dscr *op = &(td_operations(obj_type)[opindex]);
    f_status_t status;
    i_status_t i_status;
    void *msg;
    int size, len;
    invocation_p *p;
    int dummy = -1;
    invocation_t sync_inv;
#ifdef TRACING
    op_inv_info opinf;
#endif

    if (f_read(f, op_flags, op, argv, AC_LOCAL_FAST)) {
	return 1;             /* read succeeded */
    }

#ifdef TRACING
    opinf.op = op->op_index;		/* changed RFHH */
    opinf.cpu = f->fr_home;
    opinf.obj = f->fr_home_obj;
#endif
 
    status = f_get_status(f);
    if (op_read_only(op)) {
	if (f->fr_total_refs == 1) {
	    m_trap(LOCAL_DEADLOCK, (char *) 0, 0);
	}
	/* 
	 * Read failed and no write alternatives; save the invocation
	 * arguments and block until someone else completes the
	 * invocation.
	 */
	p = cont_alloc(f_get_queue(f), sizeof(*p), cont_local_invocation);
	*p = &sync_inv;
	i_init(&sync_inv, f, status, op, dummy, argv, op_flags);
	trc_event( op_block, &opinf);
	cont_save(p, 1);
	trc_event( op_cont, &opinf);
	i_status = sync_inv.result;
	i_clear(&sync_inv);
	return (i_status == I_COMPLETED);
    }

    /*
     * Read failed, but we have write alternatives: broadcast a write
     * attempt.
     */
 
#ifdef PURE_WRITE
    if (rts_is_pure_write(f->fr_type, opindex)) {
	msg = mm_pack_op_call(&size, &len, f, op, argv, &sync_inv, 1);

	start_next_write(p_self(), f);                /* try to pipeline */
	trc_event(pure_write, &opinf);

	rc_mcast(mcast_handle, msg, len);
	return 1;                                 /* trouble? */
    }
#endif

    i_init(&sync_inv, f, status, op, dummy, argv, op_flags);

    msg = mm_pack_op_call(&size, &len, f, op, argv, &sync_inv, 1);

    trc_event( bcast_block, &opinf);
    rc_mcast(mcast_handle, msg, len);

    i_status = i_block(&sync_inv);
    trc_event( bcast_cont, &opinf);	/* too late for blocking op! */
    i_clear(&sync_inv);
    return (i_status == I_COMPLETED);
}


INLINE int
remote_invocation(fragment_p f, tp_dscr *obj_type,
		  int opindex, void **argv, int *op_flags)
{
    op_dscr *op = &(td_operations(obj_type)[opindex]);
    int success, owner;
    void *request;
    int req_size, req_len;
    void *reply;
    int rep_size, rep_len;

#ifdef TRACING
    op_inv_info opinf;
#endif

#ifdef PURE_WRITE
    /*
     * We have the object at another processor P. If we have pure
     * writes pending on another object, then we wait until they're
     * done. Otherwise we have no pending writes OR we have writes
     * pending on this object.  In the latter case we can continue,
     * even if this is a read, because remote operations are performed
     * in FIFO order.
     */
    rts_flush_pure_writes(p_self(), f, 0);
#endif

/*     rts_measure_enter(marshall_req_timer); */
    owner = f_get_owner(f);
    assert(! (*op_flags & NESTED));

    /*
     * Do RPC to owner's platform
     */

    request = mm_pack_op_call(&req_size, &req_len, f, op, argv, 0, 0);

/*     rts_measure_leave(marshall_req_timer); */

#ifdef TRACING
    opinf.op = op->op_index;		/* changed RFHH */
    opinf.cpu = f->fr_home;
    opinf.obj = f->fr_home_obj;
    trc_event( rpc_block, &opinf);
#endif

    rc_rpc(owner, rpc_handle, request, req_len, &reply, &rep_size, &rep_len);
/*     rts_measure_enter(unmarshall_reply_timer); */
    mm_unpack_op_result(reply, &rep_len, op, &success, argv);

    trc_event( rpc_cont, &opinf);
    
    pan_free(request);
    pan_free(reply);
/*     rts_measure_leave(unmarshall_reply_timer); */
    if (success == OP_DEADLOCK) {
	m_trap(LOCAL_DEADLOCK, (char *) 0, 0);
    }
#ifdef ACCOUNTING
    if (success >= 1) {
	ac_tick( f, success-1, AC_LOCAL);
	success = 1;
    }
#endif
    return success;
}


INLINE int
owner_invocation(fragment_p f, tp_dscr *obj_type,
		 int opindex, void **argv, int *op_flags)
{
    op_dscr *op = &(td_operations(obj_type)[opindex]);
    cont_queue_t *cont_queue;
    int modified;
    invocation_t inv;
    invocation_p *p;
    i_status_t i_status;
    int dummy = -1;
#ifdef TRACING
    op_inv_info opinf;
#endif
 
#ifdef PURE_WRITE
    /*
       We have the object at this processor. If we have pure writes pending
       on another object, then we wait until they're done. Otherwise we
       have no pending writes OR we have writes pending on this object.
       In the latter case we can continue, even if this is a read, because
       writes on this object must have been performed synchronously.
     */
    rts_flush_pure_writes(p_self(), f, 0);
#endif

    cont_queue = f_get_queue(f);
    if (f_read_write(f, op_flags, op, argv, &modified, AC_LOCAL)) {
	if (modified && cont_pending(cont_queue)) {
	    cont_resume(cont_queue);
	}
	return 1;    	/* operation succeeded */
    }
 
    if (! (*op_flags & NESTED) && f->fr_total_refs == 1) {
	/*
	 * Top-level operations on unshared objects should never block!
	 */
	m_trap(LOCAL_DEADLOCK, (char *) 0, 0);
    }
    if (*op_flags & NESTED) {
	/* 
	 * Nested operations may not block, but return error status.
	 */
	return 0;
    }

    /*
     * Operation cannot be performed, so we save the invocation
     * arguments and block until someone else completes the invocation.
     */
    p = cont_alloc(cont_queue, sizeof(*p), cont_local_invocation);
    *p = &inv;
    i_init(&inv, f, f_get_status(f), op, dummy, argv, op_flags);
#ifdef TRACING
    opinf.op  = op->op_index;		/* changed RFHH */
    opinf.cpu = f->fr_home;
    opinf.obj = f->fr_home_obj;
#endif
    trc_event(op_block, &opinf);
    cont_save(p, 1);                    /* block now */
    trc_event( op_cont, &opinf);
 
    i_status = inv.result;
    i_clear(&inv);
    return (i_status == I_COMPLETED);
}

INLINE int
cont_await_migration(void *state)
{
    /*
     * By just returning CONT_NEXT, we ensure that
     * the operation will be retried in the DoOperation
     * loop.
     */
    return CONT_NEXT;
}

void
pr_start(void)
{
    /* 
     * Register message handlers.
     */
    rpc_handle   = rc_export(r_rpc_invocation);
    mcast_handle = rc_export(r_mcast_invocation);

/*
    marshall_reply_timer = rts_measure_create("marshall reply");
    unmarshall_req_timer = rts_measure_create("unmarshall request");
    operation_timer = rts_measure_create("operation execution");
    compiled_timer = rts_measure_create("compiled code");
    marshall_req_timer = rts_measure_create("marshall request");
    unmarshall_reply_timer = rts_measure_create("unmarshall reply");
*/
}


void
DoOperation(t_object *obj, int *op_flags, tp_dscr *obj_type, int opindex, 
			int attempted, void **argtab)
{
    fragment_p f = (fragment_p)obj;
    int ok;
#ifdef TRACING
    op_inv_info opinf;
#endif 

    if (*op_flags & BLOCKING) return;
 
/*     rts_measure_enter(client_timer); */

#ifdef TRACING
    if (f_get_status(f) != f_unshared) {
	opinf.op = opindex;		/* changed RFHH */
	opinf.cpu = f->fr_home;
	opinf.obj = f->fr_home_obj;
	trc_event( op_inv, &opinf);
    }
#endif
 
    assert(f->fr_type == obj_type);  /* yes, I'm paranoid */

    if (! (*op_flags & NESTED)) {  /* avoid recursive locking! */
	rts_lock();
    }

    for (;;) {
	switch(f_get_status(f)) {
	  case f_unshared:
	    ok = unshared_invocation(f, obj_type, opindex, argtab, op_flags);
	    break;
	  case f_replicated:
	  case f_manager:   
	    ok = replicated_invocation(f, obj_type, opindex, argtab, op_flags);
	    break;
	  case f_remote:
	    ok = remote_invocation(f, obj_type, opindex, argtab, op_flags);
	    break;
	  case f_owner:
	    ok = owner_invocation(f, obj_type, opindex, argtab, op_flags);
	    break;
	  case f_in_transit:
	    /* 
	     * We _block_ this operation until the transient state has
	     * changed. If we don't block, the DoOperation loop may
	     * starve the threads that want to change the object
	     * state.
	     *
	     * We allocate an empty continuation buffer. There is no state
	     * to store.
	     */
	    cont_save(cont_alloc(f_get_queue(f), 0, cont_await_migration), 1);
	    ok = 0;
	    break;
	  default:
	    abort();
	}

	if (ok) {
	    f->fr_info.nr_accesses++;
/* 	    rts_measure_leave(client_timer); */
	    if (! (*op_flags & NESTED)) {
		rts_unlock();
	    }
	    return;
	}

#ifdef RTS_VERBOSE
	printf("%d) (warning) status = %lx; ok = %d; invocation failed "
	       "(f = %lx, type = %lx, opindex = %d, argv = %lx\n",
	       rts_my_pid, f_get_status(f), ok, f, obj_type, opindex, argtab);
#endif

    }
}
