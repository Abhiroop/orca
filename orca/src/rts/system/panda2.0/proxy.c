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

#define OP_DEADLOCK (-1)

/*
static rts_timer_p marshall_req_timer;
static rts_timer_p unmarshall_req_timer;
static rts_timer_p marshall_reply_timer;
static rts_timer_p unmarshall_reply_timer;
static rts_timer_p operation_timer;
rts_timer_p compiled_timer;
*/

#ifdef PURE_WRITE

/* An operation is a pure write when it satisfies two conditions:
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
 * pending on another shared object. If obj == 0, then the caller
 * is always blocked, even if an operation on the same object is
 * attempted.
 */
void
rts_flush_pure_writes(process_p self, fragment_p obj, int always)
{
    assert(obj);
    pan_mutex_lock(self->p_lock);
    if (always || obj != self->p_object) {
	f_unlock(obj);
	while(self->p_nwrites > 0) {
	    pan_cond_wait(self->p_flushed);
	}
	f_lock(obj);
    }
    pan_mutex_unlock(self->p_lock);
}

/*
 * start_next_write() attempts to pipeline a new write operation.
 * The attempt fails if this is a write on another object. In this
 * case, we wait until all pending writes have been flushed.
 */
static void
start_next_write(process_p self, fragment_p obj)
{
    pan_mutex_lock(self->p_lock);
    if (self->p_nwrites == 0) {
	self->p_object = obj;           /* first in pipeline; register obj */
	self->p_pipelined++;
    } else if (obj != self->p_object) {
	/* no pipelining allowed: flush pending writes first */
	f_unlock(obj);
	while(self->p_nwrites > 0) {
	    pan_cond_wait(self->p_flushed);
	}
	f_lock(obj);
    } else {
	self->p_pipelined++;
    }
    self->p_nwrites++;                  /* register this pure write */
    pan_mutex_unlock(self->p_lock);
}

static void
pure_write_done(process_p proc)
{
    pan_mutex_lock(proc->p_lock);
    assert(proc->p_nwrites >= 1);
    if (--proc->p_nwrites == 0) {
	proc->p_object = 0;
	pan_cond_signal(proc->p_flushed);
    }
    pan_mutex_unlock(proc->p_lock);
}

#ifdef NEVER
/* Why did I ever write/copy this thing? */
static int
cont_pure_write(void *state, pan_mutex_p lock)
{
    invocation_p i = *(invocation_p *)state;
    int opindex, modified;
    tp_dscr *objtype;
    fragment_p frag;
    void **argv;
    pan_upcall_p upcall;
    f_status_t status;
    source_t src;

    i_get_info(i, &opindex, &objtype, &frag, &argv, &status, &upcall);
    if (f_get_status(frag) != status) {             /* status has changed */
	i_wakeup(i, I_FAILED);
	return CONT_NEXT;
    }
    src = (op_read_only(objtype,opindex) ? AC_LOCAL_FAST : AC_LOCAL_BC);
    if (f_write_read(frag, objtype, opindex, argv, &modified, src)) {
	pure_write_done();
	return CONT_RESTART;
    }
    return CONT_KEEP;
}
#endif

#endif

/***********************************/
/* Client-side invocation variants */
/***********************************/

static int unshared_invocation(fragment_p, tp_dscr *, int, void **, int *op_flags);
static int replicated_invocation(fragment_p, tp_dscr *, int, void **,
				 int *op_flags);
static int owner_invocation(fragment_p, tp_dscr *, int, void **,
			    int *op_flags);
static int remote_invocation(fragment_p, tp_dscr *, int, void **,
			     int *op_flags);
static int blocked_invocation(fragment_p, tp_dscr *, int, void **,
			      int *op_flags);

static int op_read_only(tp_dscr *obj_type, int opindex);


/***********************************/
/* Server-side invocation handlers */
/***********************************/

static int mcast_handle;
static int rpc_handle;


/********** Hack by Koen **************/

struct reply_cont {
    pan_upcall_p upcall;
    pan_msg_p    reply;
};

static int
send_result_cont(void *state, pan_mutex_p lock)
{
    struct reply_cont *rc;
 
    pan_mutex_unlock(lock);
    rc = (struct reply_cont *)state;
    rc_tagged_reply(rc->upcall, rc->reply);
    pan_mutex_lock(lock);
    return CONT_NEXT;
}

static void
send_result(pan_upcall_p upcall, pan_msg_p reply, int success, 
            tp_dscr *objtype, int opindex, void **argv)
{
    struct reply_cont *rc;

/*    rts_measure_enter(marshall_reply_timer); */
    mm_pack_op_result(reply, success, objtype, opindex, argv);
/*    rts_measure_leave(marshall_reply_timer); */

    if ( success) {
	rc_tagged_reply(upcall, reply);
    } else {
        /* Grrr, watch out for starvation in case where a server becomes
         * overloaded with repeatedly failing rpc requests. Have the
	 * failures send back by a thread running at low priority.
         */
        pan_mutex_lock(cont_immediate_lock);
        rc = (struct reply_cont *)cont_alloc(&cont_immediate_queue, sizeof(*rc),
                                                send_result_cont);
        rc->upcall = upcall;
        rc->reply = reply;
        cont_save(rc, 0);
        pan_mutex_unlock(cont_immediate_lock);
    }
}


/**************************************/
/*      Continuation functions        */
/**************************************/

/* All continuation functions assume that the object that they operate on 
 * has already been locked!!
 */

static int
cont_local_invocation(void *state, pan_mutex_p lock)
{
    invocation_p i = *(invocation_p *)state;
    int opindex, modified;
    tp_dscr *objtype;
    fragment_p frag;
    void **argv;
    pan_upcall_p upcall;
    f_status_t status;
    source_t src;
    int *op_flags;
    int opflags = 0;

#ifdef TRACING
    /* No "op_cont" event is generated here. It is generated just after the
     * wakeup of the thread that blocks on the guard.
     */
#endif
    i_get_info(i, &opindex, &objtype, &frag, &argv, &status, &upcall, &op_flags);
    if (! op_flags) op_flags = &opflags;
    if (f_get_status(frag) != status) {             /* status has changed */
	i_wakeup(i, I_FAILED);
	return CONT_NEXT;
    }
    src = (op_read_only(objtype,opindex) ? AC_LOCAL_FAST : AC_LOCAL_BC);
    if (f_write_read(frag, op_flags, objtype, opindex, argv, &modified, src)) {
	i_wakeup(i, I_COMPLETED);
	return (modified ? CONT_RESTART : CONT_NEXT);
    }
    return CONT_KEEP;
}


static int
cont_rpc_invocation(void *state, pan_mutex_p lock)
{
    invocation_p i = (invocation_p)state;
    int modified, opindex, success;
    tp_dscr *objtype;
    fragment_p frag;
    void **argv;
    f_status_t status;
    pan_msg_p reply;
    pan_upcall_p upcall;
    source_t src;
#ifdef TRACING
    op_cont_info continf;
#endif
    int *op_flags;
    int opflags = 0;

    i_get_info(i, &opindex, &objtype, &frag, &argv, &status, &upcall, &op_flags);
    if (! op_flags) op_flags = &opflags;

#ifdef TRACING          /* Added RAFB */
    continf.op    = opindex;
    continf.cpu   = frag->fr_home;
    continf.obj   = frag->fr_home_obj;
    continf.state = state;
#endif

    if (f_get_status(frag) != status) {              /* status has changed */
	reply = pan_msg_create();
	pan_mutex_unlock( lock);
	send_result(upcall, reply, 0, objtype, opindex, argv); 
	pan_mutex_lock( lock);
	i_clear(i);
	trc_event(cont_done, &continf);
	return CONT_NEXT;
    }
    src = (op_read_only(objtype,opindex) ? AC_REMOTE : AC_REMOTE_BC);
    if (f_write_read(frag, op_flags, objtype, opindex, argv, &modified, src)) {
	reply = pan_msg_create();
#ifdef ACCOUNTING
        success = 1 + (modified ? AC_WRITE : AC_READ);
#else
	success = 1;
#endif
	/* Bug fix by Koen: release the lock when sending a reply since
	 * the thread package may poll the network and (recursively) start an
	 * operation, which causes deadlock!
	 */
	pan_mutex_unlock( lock);
	send_result(upcall, reply, success, objtype, opindex, argv); 
	pan_mutex_lock( lock);
	i_clear(i);
	trc_event(cont_done, &continf);
	return (modified ? CONT_RESTART : CONT_NEXT);
    }
    return CONT_KEEP;
}


static int
cont_mcast_invocation(void *state, pan_mutex_p lock)
{
    invocation_p i = (invocation_p)state;
    int opindex, modified;
    tp_dscr *objtype;
    fragment_p frag;
    void **argv;
    f_status_t status;
    pan_upcall_p upcall;
    int *op_flags;
    int opflags = 0;
#ifdef TRACING
    op_cont_info continf;
#endif

    i_get_info(i, &opindex, &objtype, &frag, &argv, &status, &upcall, &op_flags);
    if (! op_flags) op_flags = &opflags;

#ifdef TRACING          /* Added RAFB */
    continf.op    = opindex;
    continf.cpu   = frag->fr_home;
    continf.obj   = frag->fr_home_obj;
    continf.state = state;
#endif

    if (f_get_status(frag) != status) {
	mm_free_op_args(objtype, opindex, argv);
	i_clear(i);
	trc_event(cont_done, &continf);
	return CONT_NEXT;
    }
    if (f_write_read(frag, op_flags, objtype, opindex, argv, &modified, AC_REMOTE_BC)) {
	mm_free_op_args(objtype, opindex, argv);
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
r_rpc_invocation(int op, pan_upcall_p upcall, pan_msg_p request)
{
    cont_queue_t *cont_queue;
    int modified, success = 0;
    op_hdr_t *hdr;
    fragment_p f;
    tp_dscr *objtype = 0;	/* Koen: use local var to avoid null
				 * dereference in case of failure
				 */
    void **argv;
    int opindex;
    man_piggy_p piggy_info;
    int opflags = 0;
    int *op_flags = &opflags;

/*    rts_measure_enter(unmarshall_req_timer); */

#ifdef PURE_WRITE
    int pure_write;
    hdr = mm_unpack_op_call(request, &f, &argv, &pure_write);
#else
    hdr = mm_unpack_op_call(request, &f, &argv, &piggy_info);
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

	objtype = f->fr_type;
	f_lock(f);
        man_unpack_piggy_info(piggy_info, f); /* inside lock, please */
/*        rts_measure_leave(unmarshall_req_timer); */
	
	cont_queue = f_get_queue(f);
	status     = f_get_status(f);
/*  	  rts_measure_enter(operation_timer); */
	if (status != f_owner) { /* not owner; data no longer present */
	    success = 0;
	} else if (f_read_write(f, op_flags, objtype, hdr->oh_opindex, argv, 
				&modified, AC_REMOTE)) {
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
	    invocation_p inv = (invocation_p)cont_alloc(cont_queue,
							sizeof(*inv),
							cont_rpc_invocation);
	    i_init(inv, f, status, objtype, hdr->oh_opindex, upcall, argv, (int *) 0);
#ifdef TRACING
	    continf.op    = hdr->oh_opindex;
	    continf.cpu   = f->fr_home;
	    continf.obj   = f->fr_home_obj;
	    continf.state = inv;
	    trc_event(cont_create, &continf);
#endif
	    cont_save(inv, use_threads);
	    f_unlock(f);
	    pan_msg_clear(request);
	    return;
	}
	f_unlock(f);
    }    

    /* trick: reuse request message to send reply */
    opindex = hdr->oh_opindex;		/* Grr, points into 'request',
					 * hence it is invalid after
					 * pan_msg_empty()
					 */
    pan_msg_empty(request);	/* In some cases the message is not
				 * processed completely, so clear it.
				 */
    send_result(upcall, request, success, objtype, opindex, argv); 
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
r_mcast_invocation(int op, pan_upcall_p dummy, pan_msg_p request)
{
    cont_queue_t *cont_queue;
    fragment_p f;
    f_status_t status;
    int modified;
    tp_dscr *obj_type;
    invocation_p inv;
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

    hdr = mm_unpack_op_call(request, &f, &argv, &pure_write);
#else
    hdr = mm_unpack_op_call(request, &f, &argv, &piggy_info);
#endif
    if (hdr->oh_src == rts_my_pid) {      /* Operation initiated locally */
        invocation_p *p;
        pan_upcall_p upcall;
        int opindex;
 
#ifdef PURE_WRITE	
	if (pure_write) {
	    proc = (process_p)hdr->oh_ptr;
	    opindex = hdr->oh_opindex;
	    obj_type = f->fr_type;
	    status = f_get_status(f);  /* hack... */
	} else {
	    /* args were not unmarshalled; fetch them 
	     * through local record
	     */
	    inv = (invocation_p)hdr->oh_ptr;
            i_get_info(inv, &opindex, &obj_type, &f, &argv, &status, &upcall, &op_flags);
	    if (! op_flags) op_flags = &opflags;
	}
#else
	inv = (invocation_p)hdr->oh_ptr;
        i_get_info(inv, &opindex, &obj_type, &f, &argv, &status, &upcall, &op_flags);
	if (! op_flags) op_flags = &opflags;
#endif

#ifdef TRACING
        opinf.op  = opindex;		/* changed RFHH */
        opinf.cpu = f->fr_home;
        opinf.obj = f->fr_home_obj;
        trc_event(op_mcast, &opinf);
#endif
        f_lock(f);
	cont_queue = f_get_queue(f);
        if (f_get_status(f) != status) {             /* status has changed */
            i_wakeup(inv, I_FAILED);
        } else if (f_write_read(f, op_flags, obj_type, opindex, argv,
				&modified, AC_LOCAL)) {
            if (modified && cont_pending(cont_queue)) {
                cont_resume(cont_queue);
            }
#ifdef PURE_WRITE
	    if (pure_write) {
		pure_write_done(proc);
		mm_free_op_args(obj_type, opindex, argv);
	    } else {
		i_wakeup(inv, I_COMPLETED);
	    }
#else
            i_wakeup(inv, I_COMPLETED);
#endif
        } else if (f->fr_total_refs == 1) {
	    m_trap(LOCAL_DEADLOCK, (char *) 0, 0);
	} else {
#ifdef PURE_WRITE
	    assert(!pure_write);
#endif
            p = (invocation_p *)cont_alloc(cont_queue, sizeof(*p),
	    				   cont_local_invocation);
            *p = inv;
            cont_save(p, 0);
#ifdef TRACING
	    continf.op    = opindex;
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
	man_op_finished(f);
	 */
        f_unlock(f);
	rc_mcast_done();
        return;
    }

    if (!f) {              /* Obj. fragment present? */
	rc_mcast_done();
        return;
    }

    f_lock(f);
    obj_type = f->fr_type;
    man_unpack_piggy_info( piggy_info, f);
    status = f_get_status(f);
    if (!(status == f_replicated || status == f_manager)) {
        f_unlock(f);
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
    if (f_write_read(f, op_flags, obj_type, hdr->oh_opindex, argv,
		     &modified, AC_REMOTE)) {
        if (modified && cont_pending(cont_queue)) {
            cont_resume(cont_queue);
        }
	man_op_finished(f);
        mm_free_op_args(obj_type, hdr->oh_opindex, argv);
    } else {
        inv = (invocation_p)cont_alloc(cont_queue, sizeof(*inv),
				       cont_mcast_invocation);
	if (op_flags == &opflags) op_flags = 0;
        i_init(inv, f, status, obj_type, hdr->oh_opindex, dummy, argv, op_flags);
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
 
    f_unlock(f);
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

static int
unshared_invocation(fragment_p f, tp_dscr *obj_type,
		    int opindex, void **argv, int *op_flags)
{
    int modified, completed;

#ifdef PURE_WRITE
    /*
       An unshared object is like a process-local variable. It can neither
       be read nor written by other processes. Therefore, we need never
       stall an operation on an unshared object, even if pure writes are
       currently in progress. (The object may become shared later; to avoid
       trouble in that case, DoFork always waits until all pending writes
       have completed.)
     */
#endif
    completed = f_read_write(f, op_flags, obj_type, opindex, argv, &modified,
			     AC_LOCAL);
    if (! (*op_flags & NESTED)) {  /* top-level? */
	f_unlock(f);
	if (!completed && f->fr_total_refs == 1) {
	    m_trap(LOCAL_DEADLOCK, (char *) 0, 0);
	}	    
    }
    return 1;
}


static int
op_read_only(tp_dscr *obj_type, int opindex)
{
    op_dscr *op;

    op = &(td_operations(obj_type)[opindex]);
    return op->op_write_alts == 0;
}


static int
replicated_invocation(fragment_p f, tp_dscr *obj_type,
		      int opindex, void **argv, int *op_flags)
{
    f_status_t status;
    i_status_t i_status;
    pan_msg_p msg;
    invocation_p *p;
    man_piggy_t piggy_info;
    pan_upcall_p dummy;
    invocation_t sync_inv;
#ifdef TRACING
    op_inv_info opinf;
#endif

    if (f_read(f, op_flags, obj_type, opindex, argv, AC_LOCAL_FAST)) {
	if (! (*op_flags & NESTED)) {
	    f_unlock(f);
	}
        return 1;             /* read succeeded */
    }

#ifdef TRACING
    opinf.op = opindex;		/* changed RFHH */
    opinf.cpu = f->fr_home;
    opinf.obj = f->fr_home_obj;
#endif
 
    status = f_get_status(f);
    if (op_read_only(obj_type, opindex)) {
        if (f->fr_total_refs == 1) {
	    m_trap(LOCAL_DEADLOCK, (char *) 0, 0);
	}
        /* Read failed and no write alternatives; save the invocation
         * arguments and block until someone else completes the invocation.
         */
        p = (invocation_p *)cont_alloc(f_get_queue(f), sizeof(*p),
	    			       cont_local_invocation);
        *p = &sync_inv;
        i_init(&sync_inv, f, status, obj_type, opindex, dummy, argv, op_flags);
    	trc_event( op_block, &opinf);
        cont_save(p, 1);
    	trc_event( op_cont, &opinf);
	if (! (*op_flags & NESTED)) {
	    f_unlock(f);
	}
	i_status = sync_inv.result;
	i_clear(&sync_inv);
        return (i_status == I_COMPLETED);
    }

    /* Read failed; broadcast a write attempt
     */
 
#ifdef PURE_WRITE
    if (rts_is_pure_write(f->fr_type, opindex)) {
	msg = pan_msg_create();
	man_get_piggy_info(f, &piggy_info);
	mm_pack_op_call(msg, f, &piggy_info, opindex, argv, &sync_inv);

	start_next_write(p_self(), f);                /* try to pipeline */
	f_unlock(f);
    	trc_event(pure_write, &opinf);

	rc_mcast(mcast_handle, msg);
	return 1;                                 /* trouble? */
    }
#endif

    i_init(&sync_inv, f, status, obj_type, opindex, dummy, argv, op_flags);
    man_get_piggy_info(f, &piggy_info);
    msg = pan_msg_create();
    mm_pack_op_call(msg, f, &piggy_info, opindex, argv, &sync_inv);

    trc_event( bcast_block, &opinf);
    f_unlock(f);
    rc_mcast(mcast_handle, msg);
    f_lock(f);

    /* Wait until the group listener has completed the operation.
     */
    i_status = i_block(&sync_inv);
    trc_event( bcast_cont, &opinf);	/* too late for blocking op! */
    i_clear(&sync_inv);
    if (! (*op_flags & NESTED)) {
	f_unlock(f);
    }
    return (i_status == I_COMPLETED);
}


static int
remote_invocation(fragment_p f, tp_dscr *obj_type,
		  int opindex, void **argv, int *op_flags)
{
    int success, owner;
    pan_msg_p request;
    pan_msg_p reply;
    man_piggy_t piggy_info;

#ifdef TRACING
    op_inv_info opinf;
#endif

#ifdef PURE_WRITE
    /*
       We have the object at another processor P. If we have pure
       writes pending on another object, then we wait until they're
       done. Otherwise we have no pending writes OR we have writes
       pending on this object.  In the latter case we can continue,
       even if this is a read, because remote operations are performed
       in FIFO order.
     */
    rts_flush_pure_writes(p_self(), f, 0);
#endif

/*     rts_measure_enter(marshall_req_timer); */
    man_get_piggy_info( f, &piggy_info);
    owner = f_get_owner(f);
    assert(! (*op_flags & NESTED));
    f_unlock(f);	/* NOW, to avoid deadlock: a blocking operation
			 * should not exclude other local threads from acessing
			 * the object, otherwise the blocking guard can not be
			 * set to true!
			 */

    /* Do RPC to owner's platform */

    request = pan_msg_create();
    mm_pack_op_call(request, f, &piggy_info, opindex, argv, 0);

/*     rts_measure_leave(marshall_req_timer); */

#ifdef TRACING
    opinf.op = opindex;		/* changed RFHH */
    opinf.cpu = f->fr_home;
    opinf.obj = f->fr_home_obj;
    trc_event( rpc_block, &opinf);
#endif

    rc_rpc(owner, rpc_handle, request, &reply);
/*     rts_measure_enter(unmarshall_reply_timer); */
    mm_unpack_op_result(reply, obj_type, opindex, &success, argv);

    trc_event( rpc_cont, &opinf);
    
    pan_msg_clear(request);
    pan_msg_clear(reply);
/*     rts_measure_leave(unmarshall_reply_timer); */
#ifdef ACCOUNTING
    if (success >= 1) {
	ac_tick( f, success-1, AC_LOCAL);
	success = 1;
    }
#endif
    if (success == OP_DEADLOCK) {
	m_trap(LOCAL_DEADLOCK, (char *) 0, 0);
    }
    return success;
}


static int
owner_invocation(fragment_p f, tp_dscr *obj_type,
		 int opindex, void **argv, int *op_flags)
{
    cont_queue_t *cont_queue;
    int modified;
    invocation_t inv;
    invocation_p *p;
    i_status_t i_status;
    pan_upcall_p dummy = NULL;
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
    if (f_read_write(f, op_flags, obj_type, opindex, argv, &modified, AC_LOCAL)) {
        if (modified && cont_pending(cont_queue)) {
            cont_resume(cont_queue);
        }
	if (! (*op_flags & NESTED)) {
	    f_unlock(f);
	}
        return 1;    	/* operation succeeded */
    }
 
    if (! (*op_flags & NESTED) && f->fr_total_refs == 1) {
	m_trap(LOCAL_DEADLOCK, (char *) 0, 0);
    }
    if (*op_flags & NESTED) {
	/* Nested operations may not block, but return error status */
	return 0;
    }

    /* Operation cannot be performed, so we save the invocation
     * arguments and block until someone else completes the invocation.
     */
    p = (invocation_p *)cont_alloc(cont_queue, sizeof(*p),
				   cont_local_invocation);
    *p = &inv;
    i_init(&inv, f, f_get_status(f), obj_type, opindex, dummy, argv, op_flags);
#ifdef TRACING
    opinf.op  = opindex;		/* changed RFHH */
    opinf.cpu = f->fr_home;
    opinf.obj = f->fr_home_obj;
#endif
    trc_event(op_block, &opinf);
    cont_save(p, 1);
    trc_event( op_cont, &opinf);
 
    if (! (*op_flags & NESTED)) {
	f_unlock(f);
    }
 
    i_status = inv.result;
    i_clear(&inv);
    return (i_status == I_COMPLETED);
}

static int
blocked_invocation(fragment_p f, tp_dscr *obj_type,
		   int opindex, void **argv, int *op_flags)
{
    man_await_migration(f);
    if (! (*op_flags & NESTED)) {
	f_unlock(f);
    }
    return 0;
}

void
pr_start(void)
{
    /* Register message handlers */
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

    for (;;) {
	if (! (*op_flags & NESTED)) {  /* avoid recursive locking! */
	    f_lock(f);
	}
 
        /*
         * Call one of the following functions:  unshared_invocation(),
         * replicated_invocation(), manager_invocation(),
         * owner_invocation(), remote_invocation().
         */
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
	    ok = blocked_invocation(f, obj_type, opindex, argtab, op_flags);
	    break;
	  default:
	    abort();
	}

        if (ok) {
	    f->fr_info.nr_accesses++;
/* 	    rts_measure_leave(client_timer); */
            return;
        } else {
	    /*
            printf("%d) (warning) status = %lx; ok = %d; invocation failed "
                   "(f = %lx, type = %lx, opindex = %d, argv = %lx\n",
		   rts_my_pid, status, ok, f, obj_type, opindex, argtab);
	    */
        }
    }
}
