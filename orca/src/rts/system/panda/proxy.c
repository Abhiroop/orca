#include "panda/panda.h"
#include "orca_types.h"
#include "continuation.h"
#include "fragment.h"
#include "invocation.h"
#include "manager.h"
#include "msg_marshall.h"
#include "obj_tab.h"
#include "proxy.h"
#include "rts_comm.h"
#include "rts_trace.h"
#include "account.h"

/*************************/
/* Communication details */
/*************************/

extern mpool_t message_pool;

static int proxy_handle;          /* invocation service handle */



/***********************************/
/* Client-side invocation variants */
/***********************************/

static int unshared_invocation(fragment_p, tp_dscr *, int, void **);
static int replicated_invocation(fragment_p, tp_dscr *, int, void **);
static int owner_invocation(fragment_p, tp_dscr *, int, void **);
static int remote_invocation(fragment_p, tp_dscr *, int, void **);
static int blocked_invocation(fragment_p, tp_dscr *, int, void **);

static int (*invocation_variant[F_MAX_STATUS])
     (fragment_p f, tp_dscr *obj_type, int opindex, void **argv) = {NULL};

static int op_read_only(tp_dscr *obj_type, int opindex);



/***********************************/
/* Server-side invocation handlers */
/***********************************/

#define MCAST_INVOCATION       0
#define RPC_INVOCATION         1

static void r_mcast_invocation(int op, pan_upcall_t, message_p request);
static void r_rpc_invocation(int op, pan_upcall_t upcall, message_p request);
 
static void (*invocation_handlers[]) (int, pan_upcall_t, message_p) = {
    r_mcast_invocation,      /* MCAST_INVOCATION */
    r_rpc_invocation         /* RPC_INVOCATION   */
};


/********** Hack by Koen **************/

struct reply_cont {
    pan_upcall_t upcall;
    message_p reply;
};

static int
send_result_cont(void *state, mutex_t *lock)
{
    struct reply_cont *rc;
 
    sys_mutex_unlock( lock);
    rc = (struct reply_cont *)state;
    rc_tagged_reply(rc->upcall, rc->reply);
    sys_mutex_lock( lock);
    return CONT_NEXT;
}

static void
send_result(pan_upcall_t upcall, message_p reply, int success, 
            tp_dscr *objtype, int opindex, void **argv)
{
    struct reply_cont *rc;

    mm_pack_op_result(reply, success, objtype, opindex, argv);
    if ( success) {
	rc_tagged_reply(upcall, reply);
    } else {
        /* Grrr, watch out for starvation in case where a server becomes
         * overloaded with repeatedly failing rpc requests. Have the
	 * failures send back by a thread running at low priority.
         */
        sys_mutex_lock( &cont_immediate_lock);
        rc = (struct reply_cont *)cont_alloc(&cont_immediate_queue, sizeof(*rc),
                                                send_result_cont);
        rc->upcall = upcall;
        rc->reply = reply;
        cont_save(rc, 0);
        sys_mutex_unlock( &cont_immediate_lock);
    }
}


/**************************************/
/*      Continuation functions        */
/**************************************/

/* All continuation functions assume that the object that they operate on 
 * has already been locked!!
 */

static int
cont_local_invocation(void *state, mutex_p lock)
{
    invocation_p i = *(invocation_p *)state;
    int opindex, modified;
    tp_dscr *objtype;
    fragment_p frag;
    void **argv;
    pan_upcall_t upcall;
    f_status_t status;
    source_t src;

    i_get_info(i, &opindex, &objtype, &frag, &argv, &status, &upcall);
    if (f_get_status(frag) != status) {             /* status has changed */
	i_wakeup(i, I_FAILED);
	return CONT_NEXT;
    }
    src = (op_read_only( objtype, opindex) ? AC_LOCAL_FAST : AC_LOCAL);
    if (f_write_read(frag, objtype, opindex, argv, &modified, src)) {
	i_wakeup(i, I_COMPLETED);
	return (modified ? CONT_RESTART : CONT_NEXT);
    }
    return CONT_KEEP;
}


static int
cont_rpc_invocation(void *state, mutex_p lock)
{
    invocation_p i = (invocation_p)state;
    int modified, opindex, success;
    tp_dscr *objtype;
    fragment_p frag;
    void **argv;
    f_status_t status;
    message_p reply;
    pan_upcall_t upcall;
#ifdef TRACING
    op_cont_info continf;
#endif

    i_get_info(i, &opindex, &objtype, &frag, &argv, &status, &upcall);

#ifdef TRACING          /* Added RAFB */
    continf.op    = opindex;
    continf.cpu   = frag->fr_oid.cpu;
    continf.obj   = frag->fr_oid.rts;
    continf.state = state;
#endif

    if (f_get_status(frag) != status) {              /* status has changed */
	reply = get_mpool(&message_pool);
	send_result(upcall, reply, 0, objtype, opindex, argv); 
	i_clear(i);
	trc_event(cont_done, &continf);
	return CONT_NEXT;
    }
    if (f_write_read(frag, objtype, opindex, argv, &modified, AC_REMOTE)) {
	reply = get_mpool(&message_pool);
#ifdef ACCOUNTING
        success = 1 + (modified ? AC_WRITE : AC_READ);
#else
	success = 1;
#endif
	send_result(upcall, reply, success, objtype, opindex, argv); 
	i_clear(i);
	trc_event(cont_done, &continf);
	return (modified ? CONT_RESTART : CONT_NEXT);
    }
    return CONT_KEEP;
}


static int
cont_mcast_invocation(void *state, mutex_p lock)
{
    invocation_p i = (invocation_p)state;
    int opindex, modified;
    tp_dscr *objtype;
    fragment_p frag;
    void **argv;
    f_status_t status;
    pan_upcall_t upcall;
#ifdef TRACING
    op_cont_info continf;
#endif

    i_get_info(i, &opindex, &objtype, &frag, &argv, &status, &upcall);

#ifdef TRACING          /* Added RAFB */
    continf.op    = opindex;
    continf.cpu   = frag->fr_oid.cpu;
    continf.obj   = frag->fr_oid.rts;
    continf.state = state;
#endif
 

    if (f_get_status(frag) != status) {
	mm_free_op_args(objtype, opindex, argv);
	i_clear(i);
	trc_event(cont_done, &continf);  /* Added RAFB */
	return CONT_NEXT;
    }
    if (f_write_read(frag, objtype, opindex, argv, &modified, AC_REMOTE)) {
	mm_free_op_args(objtype, opindex, argv);
	i_clear(i);
	trc_event(cont_done, &continf);  /* Added RAFB */
	return (modified ? CONT_RESTART : CONT_NEXT);
    }
    return CONT_KEEP;
}


/**************************************/
/* remote invocation service routines */
/**************************************/


static void 
r_rpc_invocation(int op, pan_upcall_t upcall, message_p request)
{
    cont_queue_t *cont_queue;
    tp_dscr *obj_type;
    int modified, success = 0;
    op_hdr_t *hdr;
    fragment_p f;
    void **argv;
    int opindex;

    hdr = mm_unpack_op_call(request, &f, &obj_type, &argv);
    
    if (f) {
	f_status_t status;
#ifdef TRACING
	op_inv_info opinf;
	op_cont_info continf;
	opinf.op  = hdr->oh_opindex;		/* changed RFHH */
        opinf.cpu = f->fr_oid.cpu;
        opinf.obj = f->fr_oid.rts;
	trc_event(op_rpc, &opinf);
#endif

	f_lock(f);
        man_unpack_piggy_info(request, f); /* inside lock, please */

	cont_queue = f_get_queue(f);
	status     = f_get_status(f);
	if (status != f_owner) { /* not owner; data no longer present */
	    success = 0;
	} else if (f_read_write(f, obj_type, hdr->oh_opindex, argv, 
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
	} else {
	    invocation_p inv = (invocation_p)cont_alloc(cont_queue,
							sizeof(*inv),
							cont_rpc_invocation);
	    i_init(inv, f, status, obj_type, hdr->oh_opindex, upcall, argv);
#ifdef TRACING
	    continf.op    = hdr->oh_opindex;
	    continf.cpu   = f->fr_oid.cpu;
	    continf.obj   = f->fr_oid.rts;
	    continf.state = inv;
	    trc_event(cont_create, &continf);
#endif
#ifdef BLOCKING_UPCALLS
	    cont_save(inv, 1);
#else
	    cont_save(inv, 0);
#endif

	    f_unlock(f);
	    sys_message_clear(request);
	    return;
	}
	f_unlock(f);
    }    

    /* trick: reuse request message to send reply */
    opindex = hdr->oh_opindex;		/* Grr, points into 'request',
					 * hence it is invalid after
					 * sys_message_empty()
					 */
    sys_message_empty( request);	/* In some cases the message is not
    					 * processed completely, so clear it.
					 */
    send_result(upcall, request, success, obj_type, opindex, argv); 
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
r_mcast_invocation(int op, pan_upcall_t dummy, message_p request)
{
    cont_queue_t *cont_queue;
    fragment_p f;
    f_status_t status;
    int modified;
    tp_dscr *obj_type;
    invocation_p inv;
    op_hdr_t *hdr;
    void **argv;
#ifdef TRACING
    op_inv_info opinf;
    op_cont_info continf;
#endif
 
    hdr = mm_unpack_op_call(request, &f, &obj_type, &argv);
 
    if (hdr->oh_src == sys_my_pid) {      /* Operation initiated locally */
        invocation_p *p;
        pan_upcall_t upcall;
        int opindex;
 
	inv = hdr->oh_inv;
        i_get_info(inv, &opindex, &obj_type, &f, &argv, &status, &upcall);
#ifdef TRACING
        opinf.op  = opindex;		/* changed RFHH */
        opinf.cpu = f->fr_oid.cpu;
        opinf.obj = f->fr_oid.rts;
        trc_event(op_mcast, &opinf);
#endif
        f_lock(f);
	cont_queue = f_get_queue(f);
        if (f_get_status(f) != status) {             /* status has changed */
            i_wakeup(inv, I_FAILED);
        } else if (f_write_read(f, obj_type, opindex, argv,
				&modified, AC_LOCAL)) {
            if (modified && cont_pending(cont_queue)) {
                cont_resume(cont_queue);
            }
            i_wakeup(inv, I_COMPLETED);
        } else {
            p = (invocation_p *)cont_alloc(cont_queue, sizeof(*p),
	    				   cont_local_invocation);
            *p = inv;
            cont_save(p, 0);
#ifdef TRACING
	    continf.op    = opindex;
	    continf.cpu   = f->fr_oid.cpu;
	    continf.obj   = f->fr_oid.rts;
	    continf.state = p;
	    trc_event(cont_create, &continf);
#endif
        }
	man_op_finished(f);
        f_unlock(f);
	rc_mcast_done();
        return;
    }

    if (!f) {              /* Obj. fragment present? */
	rc_mcast_done();
        return;
    }
    f_lock(f);
    man_unpack_piggy_info( request, f);
    status = f_get_status(f);
    if (!(status == f_replicated || status == f_manager)) {
        f_unlock(f);
        oid_clear(&oid);
	rc_mcast_done();
        return;
    }
 
#ifdef TRACING
    opinf.op = hdr->oh_opindex;		/* changed RFHH */
    opinf.cpu = f->fr_oid.cpu;
    opinf.obj = f->fr_oid.rts;
    trc_event( op_mcast, &opinf);
#endif
 
    cont_queue = f_get_queue(f);
    if (f_write_read(f, obj_type, hdr->oh_opindex, argv,
		     &modified, AC_REMOTE)) {
        if (modified && cont_pending(cont_queue)) {
            cont_resume(cont_queue);
        }
	man_op_finished(f);
        mm_free_op_args(obj_type, hdr->oh_opindex, argv);
    } else {
        inv = (invocation_p)cont_alloc(cont_queue, sizeof(*inv),
				       cont_mcast_invocation);
        i_init(inv, f, status, obj_type, hdr->oh_opindex, dummy, argv);
        cont_save(inv, 0);
#ifdef TRACING
	continf.op    = hdr->oh_opindex;
	continf.cpu   = f->fr_oid.cpu;
	continf.obj   = f->fr_oid.rts;
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
				 int opindex, void **argv)
{
    int modified;

    (void)f_read_write(f, obj_type, opindex, argv, &modified, AC_LOCAL);
    f_unlock(f);
    return 1;
}


static int
op_read_only(tp_dscr *obj_type, int opindex)
{
    switch(opindex) {
      case READOBJ: {
	  return(1);
      }

      case WRITEOBJ: {
	  return(0);
      }
	
      default: {
	  op_dscr *op    = &(td_operations(obj_type)[opindex-2]);
	  
	  return(op->op_write_alts == 0);
      }
    }
}


static int
replicated_invocation(fragment_p f, tp_dscr *obj_type,
				   int opindex, void **argv)
{
    f_status_t status;
    i_status_t i_status;
    message_p msg;
    invocation_t inv;
    invocation_p *p;
    oid_t oid;
    man_piggy_t piggy_info;
#ifdef TRACING
    op_inv_info opinf;
#endif
    pan_upcall_t dummy;

    if (f_read(f, obj_type, opindex, argv, AC_LOCAL_FAST)) {
        f_unlock(f);
        return 1;             /* read succeeded */
    }

#ifdef TRACING
    opinf.op = opindex;		/* changed RFHH */
    opinf.cpu = f->fr_oid.cpu;
    opinf.obj = f->fr_oid.rts;
#endif
 
    status = f_get_status(f);
    if (op_read_only( obj_type, opindex)) {
        /* Read failed and no write alternatives; save the invocation
         * arguments and block until someone else completes the invocation.
         */
        p = (invocation_p *)cont_alloc(f_get_queue(f), sizeof(*p),
	    			       cont_local_invocation);
        *p = &inv;
        i_init(&inv, f, status, obj_type, opindex, dummy, argv);
    	trc_event( op_block, &opinf);
        cont_save(p, 1);
    	trc_event( op_cont, &opinf);
        f_unlock(f);
        i_status = inv.result;
        i_clear(&inv);
        return (i_status == I_COMPLETED);
    }
    /* Read failed; broadcast a write attempt
     */
    i_init(&inv, f, status, obj_type, opindex, dummy, argv);
    f_get_oid(f, &oid);
    man_get_piggy_info( f, &piggy_info);
    msg = grp_message_init();
    mm_pack_op_call(msg, &oid, &piggy_info, obj_type, opindex, argv, &inv);
 
    trc_event( bcast_block, &opinf);
    f_unlock(f);
    rc_mcast(proxy_handle, MCAST_INVOCATION, msg);
    f_lock(f);
 
    /* Wait until the group listener has completed the operation.
     */
    i_status = i_block(&inv, f_get_lock(f));
    trc_event( bcast_cont, &opinf);	/* too late for blocking op! */
    i_clear(&inv);
    f_unlock(f);
 
    return (i_status == I_COMPLETED);
}


static int
remote_invocation(fragment_p f, tp_dscr *obj_type,
			      int opindex, void **argv)
{
    int success, owner;
    message_p request;
    message_p reply;
    oid_t oid;
    man_piggy_t piggy_info;

#ifdef TRACING
    op_inv_info opinf;
#endif

    f_get_oid(f, &oid);
    man_get_piggy_info( f, &piggy_info);
    owner = f_get_owner(f);
    f_unlock(f);	/* NOW, to avoid deadlock: a blocking operation
			 * should not exclude other local threads from acessing
			 * the object, otherwise the blocking guard can not be
			 * set to true!
			 */
    /* Do RPC to owner's platform */

    request = get_mpool(&message_pool);
    mm_pack_op_call(request, &oid, &piggy_info, obj_type, opindex, argv, NULL);

#ifdef TRACING
    opinf.op = opindex;		/* changed RFHH */
    opinf.cpu = f->fr_oid.cpu;
    opinf.obj = f->fr_oid.rts;
    trc_event( rpc_block, &opinf);
#endif

    rc_rpc(owner, proxy_handle, RPC_INVOCATION, request, &reply);
    mm_unpack_op_result(reply, obj_type, opindex, &success, argv);

    trc_event( rpc_cont, &opinf);
    
    sys_message_clear(request);
    sys_message_clear(reply);
#ifdef ACCOUNTING
    if (success >= 1) {
	ac_tick( f, success-1, AC_LOCAL);
	success = 1;
    }
#endif
    return success;
}


static int
owner_invocation(fragment_p f, tp_dscr *obj_type,
			     int opindex, void **argv)
{
    cont_queue_t *cont_queue;
    int modified;
    invocation_t inv;
    invocation_p *p;
    i_status_t i_status;
    pan_upcall_t dummy;
#ifdef TRACING
    op_inv_info opinf;
#endif
 
    cont_queue = f_get_queue(f);
    if (f_read_write(f, obj_type, opindex, argv, &modified, AC_LOCAL)) {
        if (modified && cont_pending(cont_queue)) {
            cont_resume(cont_queue);
        }
        f_unlock(f);
        return 1;    	/* operation succeeded */
    }
 
    /* Operation cannot be performed, so we save the invocation
     * arguments and block until someone else completes the invocation.
     */
    p = (invocation_p *)cont_alloc(cont_queue, sizeof(*p),
				   cont_local_invocation);
    *p = &inv;
    i_init(&inv, f, f_get_status(f), obj_type, opindex, dummy, argv);
#ifdef TRACING
    opinf.op  = opindex;		/* changed RFHH */
    opinf.cpu = f->fr_oid.cpu;
    opinf.obj = f->fr_oid.rts;
#endif
    trc_event( op_block, &opinf);
    cont_save(p, 1);
    trc_event( op_cont, &opinf);
 
    f_unlock(f);
 
    i_status = inv.result;
    i_clear(&inv);
    return (i_status == I_COMPLETED);
}

static int
blocked_invocation(fragment_p f, tp_dscr *obj_type,
			      int opindex, void **argv)
{
    man_await_migration(f);
    f_unlock(f);
    return 0;
}

void
pr_start(void)
{
    /* Initialise array of invocation variants */
    invocation_variant[f_unshared]   = unshared_invocation;
    invocation_variant[f_replicated] = replicated_invocation;
    invocation_variant[f_manager]    = replicated_invocation;
    invocation_variant[f_remote]     = remote_invocation;
    invocation_variant[f_owner]      = owner_invocation;
    invocation_variant[f_in_transit] = blocked_invocation;

    /* Get handle to invocation services */
    proxy_handle = rc_export("operation", 2, invocation_handlers);
}


void
DoOperation(t_object *obj, tp_dscr *obj_type, int opindex, 
                        int attempted, void **argtab)
{
    fragment_p f = (fragment_p)obj;
    f_status_t status;
    int ok;
#ifdef TRACING
    op_inv_info opinf;
#endif 
 
#ifdef TRACING
    opinf.op = opindex;		/* changed RFHH */
    opinf.cpu = f->fr_oid.cpu;
    opinf.obj = f->fr_oid.rts;
    trc_event( op_inv, &opinf);
#endif
 
 
    /* ignore attempted. The object's status is used as an index
     * to find the right invocation variant.
     */
 
    for (;;) {
        f_lock(f);
        status = f_get_status(f);
 
        /*
         * Call one of the following functions:  unshared_invocation(),
         * replicated_invocation(), manager_invocation(),
         * owner_invocation(), remote_invocation().
         */
        ok = (*invocation_variant[status])(f, obj_type, opindex, argtab);
        if (ok) {
	    f->fr_info.nr_accesses++;
            return;
        } else {
            /* printf("%d) (warning) status = %lx; ok = %d; invocation failed "
                   "(f = %lx, type = %lx, opindex = %d, argv = %lx\n",
		   sys_my_pid, status, ok, f, obj_type, opindex, argtab); */
        }
    }
}
