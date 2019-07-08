/* $Id: operation.c,v 1.31 1996/07/04 08:52:59 ceriel Exp $ */

#include <interface.h>
#include "scheduler.h"
#include "remote.h"
#include "trace_rec.h"
#include "policy.h"
#include "message.h"
#include "thread.h"
#include "module/syscall.h"
#include "module/mutex.h"

#define TRYAGAIN	(-1)

/* This file implements operations on objects. Two strategies are implemented
 * for operations on shared objects: 1) a object is replicated on all cpus that
 * use it; 2) object is stored on one specific processor (called the owner).
 */    
    
extern int maxmesssize;
extern int NoForkYet;
extern int checkpointing;
extern FILE *file;
extern int this_cpu;
extern int ncpus;
extern int trace_reads;
extern int rts_verbose;
extern int do_strategy;

struct opstat opstat;

static trace_rec_t op_trace;	/* buffer for trace info */

/* DEBUG */
static mutex mu;
static int minit = 0;

static int global_operation(t_object *obj, int lock, op_dscr *op, void **argtab);

void
__Score(void *data, tp_dscr *d, double score, double naccess, double uncertainty)
{
#ifdef OPTIMIZED
    switch(d->td_type) {
    case OBJECT: {
	register t_object *obj = data;

	assert(obj->o_state);
	obj->o_state[this_cpu].o_score = score;
	obj->o_state[this_cpu].o_naccess = naccess;
	obj->o_state[this_cpu].o_uncertainty = uncertainty;
	break;
	}
    case ARRAY: {
    	t_array *a = data;
	char *p;
	register int i;
	
	a = data;
	d = td_elemdscr(d);
	p = &((char *)(a->a_data))[a->a_offset * d->td_size];
	for(i = a->a_sz; i > 0; i--) {
	    __Score(p, d, score/a->a_sz, naccess/a->a_sz, uncertainty/a->a_sz);
	    p += d->td_size;
	}
	break;
	}
    case RECORD:
    case GRAPH:
	/* ??? */
	break;
    }
#endif
}

/* Tells which strategy to use for obj. */
void
m_strategy(t_object *obj, int replicated, int owner)
{
    if (! do_strategy) return;
    if(!checkpointing) {
	if (obj->o_shared) {
		m_liberr("Run-time error", "Strategy call on object that is already shared");
	}
	obj->o_replicated = replicated;
	obj->o_strategy_set = 1;
	obj->o_owner = owner;
	if (rts_verbose > 1) {
		fprintf(file,
			"obj (%X, %d) replicated %d owner %d\n",
			obj, obj->o_id, replicated, owner);
		fflush(file);
	}
    }
    else {
	fprintf(file, "warning: checkpointing enabled; Strategy call ignored\n");
    }
}

static void
print_waitinglist(t_object *x)
{
    register t_waiting *wp;
    int i;

    fprintf(file, "waiting list:\n");
    for (wp = x->o_waitinglist; wp != 0; wp = wp->w_next) {
	if (wp->w_cnt == 0) break;
	for (i = 0; i < wp->w_cnt; i++) {
	    fprintf(file, "%d %d\n", wp->w_cpu[i], wp->w_process[i]);
	}
    }
}


/* A thread puts itself on the list of threads that is blocked on object x. */
void
suspend_on(t_object *x, process_p p)
{
    register t_waiting *wp;
    
    for (wp = x->o_waitinglist; wp != 0 && wp->w_cnt == WCHUNK; wp = wp->w_next);
    if (wp == 0 ) {
	wp = (t_waiting *) m_malloc(sizeof(struct waiting));
	wp->w_cnt = 0;
	wp->w_next = x->o_waitinglist;
	x->o_waitinglist = wp;
    }
    wp->w_cpu[wp->w_cnt] = this_cpu;
    assert(sizeof(char *) == sizeof(int));
    wp->w_process[wp->w_cnt] = p;
    wp->w_cnt++;
}


/* The state of object x changed; wakeup all threads up that are waiting for
 * this event. */
void
touched(t_object *x)
{
    register t_waiting *wp;
    register int i;

    for (wp = x->o_waitinglist; wp != 0; wp = wp->w_next) {
	if (wp->w_cnt == 0) break;
	for (i = 0; i < wp->w_cnt; i++) {
	    WakeUp(wp->w_cpu[i], wp->w_process[i]);
	}
	wp->w_cnt = 0;
    }
}

void
chk_size(len, name)
	int	len;
	char	*name;
{
	if (len >= maxmesssize) {
		char buf[256];
		sprintf(buf, "Orca bcast rts (%s) marshall too many arguments: %d", name, len);
		m_syserr(buf);
	}
}

/* broadcast operation. */
static int
DistributeOperation(process_p me, t_object *obj, tp_dscr *d, int opindex, void **argtab)
{
    int len, r;
    op_dscr *op = &(td_operations(d)[opindex]);

    if (ncpus == 1 || NoForkYet) {
	r = global_operation(obj, 0, op, argtab);
    } else {
	len = 4*sizeof(int) + (*(op->op_size_op_call))(argtab);
	chk_size(len, "DistributeOperation");
	memcpy(me->buf, &(td_registration(d)), sizeof(int));
	memcpy(me->buf+sizeof(int), &opindex, sizeof(int));
	memcpy(me->buf+2*sizeof(int), &(obj->o_id), sizeof(int));
	memcpy(me->buf+3*sizeof(int), &argtab, sizeof(int));	/* YECH */
	(void)(*(op->op_marshall_op_call))(me->buf+4*sizeof(int), argtab);
	mu_unlock(&obj->o_mutex);
	grp_update(UPDATE, obj->o_id, me->buf, len, me);
	mu_lock(&obj->o_mutex);
	r =  me->ResultOperation;
    }
    return(r);
}


/* Perform operation at the owner. */
static int
RemoteOperation(process_p me, t_object *obj, tp_dscr *d, int opindex, void **argtab)
{
    int len;
    int replen;
    int r = FALSE;
    op_dscr *op = &(td_operations(d)[opindex]);
    
    assert(ncpus > 1);
    assert(this_cpu != obj->o_owner);
    len = 4*sizeof(int) + (*(op->op_size_op_call))(argtab);
    chk_size(len, "RemoteOperation");
    memcpy(me->buf, &(td_registration(d)), sizeof(int));
    memcpy(me->buf+sizeof(int), &opindex, sizeof(int));
    memcpy(me->buf+2*sizeof(int), &(obj->o_id), sizeof(int));
    memcpy(me->buf+3*sizeof(int), &argtab, sizeof(int));	/* YECH */
    (void)(*(op->op_marshall_op_call))(me->buf+4*sizeof(int), argtab);
    mu_unlock(&obj->o_mutex);
    r = rpc_doop(OPERATION, obj->o_owner, me->buf, len, &replen);
    mu_lock(&obj->o_mutex);
    assert(r == TRUE || r == TRYAGAIN);
    if (r == TRUE) (void) (*(op->op_unmarshall_op_return))(me->buf, argtab);
    return (r);
}

#define MAXARGS 50

/* Operation on non-shared or nested object */
static void
local_operation(int *op_flags, t_object *obj, tp_dscr *d, int opindex, void **argtab)
{
    register int i;
    op_dscr *op;
    register t_object *tmp;

    op = &(td_operations(d)[opindex]);
    *op_flags &= ~BLOCKING;
    if (op->op_read_alts) {
	if ((*op->op_read_alts)(obj, argtab)) {
	    if (op->op_write_alts) {
		*op_flags &= ~BLOCKING;
		if ((*op->op_write_alts)(obj, argtab)) {
			*op_flags |= BLOCKING;
		}
	    }
	}
    }
    else {
	if ((*op->op_write_alts)(obj, argtab)) {
		*op_flags |= BLOCKING;
	}
    }
    if ((*op_flags & BLOCKING) && ! (*op_flags & NESTED)) {
	m_trap(LOCAL_DEADLOCK, (char *) 0, 0);
    }
}


/* Perform read operation on obj */
static int
r_global_operation(t_object *obj, op_dscr *op, void **argtab)
{
    if (! (*op->op_read_alts)(obj, argtab)) {
	OBJ_READ_INC(obj);
	return TRUE;
    }
    return FALSE;
}


/* Operation on shared object. Only lock the object when the caller
 * says so. 
 */
static int
global_operation(t_object *obj, int lock, op_dscr *op, void **argtab)
{
    register int i;
    register t_object *tmp;
    int noblock = FALSE;

    if(lock) mu_lock(&obj->o_mutex);

    if (op->op_read_alts) {
	if (! (*op->op_read_alts)(obj, argtab)) {
		OBJ_READ_INC(obj);
		noblock = TRUE;
	}
	else if (op->op_write_alts) {
		if (! (*op->op_write_alts)(obj, argtab)) {
	    		touched(obj);
	    		OBJ_WRITE_INC(obj);
			noblock = TRUE;
		}
	}
    }
    else {
	if (! (*op->op_write_alts)(obj, argtab)) {
		touched(obj);
	    	OBJ_WRITE_INC(obj);
		noblock = TRUE;
	}
    }
    if(lock) mu_unlock(&obj->o_mutex);
    return noblock;
}


/* Receive an update on a replicated object. */
void
r_update(hdr, request, reqlen)
    header *hdr;
    char *request;
    int reqlen;
{
    int cpu;
    int r;
    void **act_param;
    int id;
    int descr_reg;
    int opindex;
    t_object *o;
    t_object *GetObjectDescr();
    tp_dscr *d;
    op_dscr *op;
    process_p proc = 0;
    trace_rec_t upd_trace;	/* buffer for trace info */
	/* If the op_trace buffer would be used here, there is a race
	 * condition between with the Orca user-threads.
	 */
    

    cpu = hdr->h_extra;
    if (this_cpu == cpu) {
	proc = (process_p) hdr->h_offset;
    }
    memcpy(&descr_reg, request, sizeof(int));
    memcpy(&opindex, request+sizeof(int), sizeof(int));
    memcpy(&id, request+2*sizeof(int), sizeof(int));
    d = m_getptr(descr_reg);
    op = &(td_operations(d)[opindex]);
    if (o = GetObjectDescr(id)) {
	if(!o->o_replicated) {
	    /* When the update was issued the object was replicated, but now
	     * the object is no longer replicated. The cpu that issued the
	     * update should perform an RPC.
	     */
	    if(this_cpu == cpu) {
		assert(proc == (process_p) hdr->h_offset);
		proc->ResultOperation = TRYAGAIN;
	    }
	} else {
	    /* Object descriptor exists and we also have a replica. */
#ifdef OPTIMIZED
	    assert(o->o_state);
	    assert(o->o_state[this_cpu].o_replica);
#endif
	    if (cpu == this_cpu) {
		memcpy(&act_param, request+3*sizeof(int), sizeof(int));	/* YECH */
	        r = global_operation(o, 1, op, act_param);
		assert(proc == (process_p) hdr->h_offset);
		proc->ResultOperation = r;
		if(!r) {
    			mu_lock(&o->o_mutex);
			suspend_on(o, proc);
    			mu_unlock(&o->o_mutex);
		}
	    }
	    else {
		(void)(*(op->op_unmarshall_op_call))(request+4*sizeof(int), &act_param);
		if (op) put_op_rec(&upd_trace, cpu, op->op_func, o, o->o_id, 0, UPDATE_OPER);
	        r = global_operation(o, 1, op, act_param);
		(*(op->op_free_op_return))(act_param);
	    }
	}
	if (this_cpu == cpu) {	/* notify thread that operation is performed */
	    assert(proc);
	    proc->received = 1;
	    sema_up(&proc->SendDone);
	}
    }
}



/* Perform an operation on a replicated object. */
static int
brc_operation(process_p me, int *op_flags, t_object *obj, tp_dscr *d, int opindex, void **argtab)
{
    int local = ! trace_reads;
    int r;
    op_dscr *op = 0;
    int trace_begin_generated = 0;

    op = &((td_operations(d))[opindex]);
    mu_lock(&obj->o_mutex);		/* lock object */
    while(obj->o_replicated) {
	*op_flags &= ~BLOCKING;
	if (op->op_read_alts) {
		BSTINC(oo_nread_trial);
		if (op && ! local && ! trace_begin_generated) {
			trace_begin_generated = 1;
	    		put_op_rec(&op_trace, this_cpu, op->op_func, obj, obj->o_id,
	    	   		me->process_id, BROADCAST_OPER);
		}
		if (r_global_operation(obj, op, argtab)) {
	    		/* shared read operation succeeded */
	    		BSTINC(oo_nread);
	    		if (op && !local) {
				put_operexit_rec(&op_trace, this_cpu, op->op_func,
				     obj, obj->o_id,
				     me->process_id, BROADCAST_OPER, 0);
			}
	    		mu_unlock(&obj->o_mutex);	/* done; unlock */
	    		return TRUE;
		}
	}
	*op_flags &= ~BLOCKING;
	if (op->op_write_alts) {
	    if (local) local = 0;
	    if (op && ! trace_begin_generated) {
		trace_begin_generated = 1;
		put_op_rec(&op_trace, this_cpu, op->op_func, obj, obj->o_id,
	    		   me->process_id, BROADCAST_OPER);
	    }
	    BSTINC(oo_nwrite_trial);
	    if((r = DistributeOperation(me, obj, d, opindex, argtab)) == TRUE){  
	        /* shared write operation succeeded */
	        BSTINC(oo_nwrite);
		if (op) put_operexit_rec(&op_trace, this_cpu, op->op_func, obj, obj->o_id,
				     me->process_id, BROADCAST_OPER, 1);
		mu_unlock(&obj->o_mutex);	/* done; unlock */
	        return TRUE;
	    } else if (r == TRYAGAIN) {
		/* The object changed from owner. */
		mu_unlock(&obj->o_mutex);
		return r;
	    }
	    *op_flags |= BLOCKING;
	    /* operation failed; block */
	}
	if (NoForkYet || ! op->op_write_alts) {
	    suspend_on(obj, me);
	}
	if (local) {
	     local = 0;
	     if (op && ! trace_begin_generated) {
		trace_begin_generated = 1;
		put_op_rec(&op_trace, this_cpu, op->op_func, obj, obj->o_id, 
	     		me->process_id, BROADCAST_OPER);
	    }
	}
	if (op) put_suspend_rec(&op_trace, this_cpu, op->op_func, obj, obj->o_id,
			me->process_id, BROADCAST_OPER);
 	mu_unlock(&obj->o_mutex);		/* sleep; unlock object */
	go_to_sleep(me);
	*op_flags &= ~BLOCKING;
        mu_lock(&obj->o_mutex);		/* lock object */
    }
    /* The object changed from replicated to unreplicated; try an RPC
     * the owner of the object.
     */
    mu_unlock(&obj->o_mutex);
    return TRYAGAIN;
}


/* Receive an operation on a non-replicated object and that is owned
 * by this cpu.
 */
void
r_operation(hdr, request, reqlen, reply, replen)
    header *hdr;
    char *request;
    int reqlen;
    char **reply;
    int *replen;
{
    int id;
    int r;
    int i;
    void **argv;
    int opindex;
    int descr_reg;
    t_object *o;
    t_object *GetObjectDescr();
    tp_dscr *d;
    process_p me = GET_TASKDATA();
    op_dscr *op;
    
    assert(this_cpu != hdr->h_extra);

    memcpy(&descr_reg, request, sizeof(int));
    memcpy(&opindex, request+sizeof(int), sizeof(int));
    memcpy(&id, request+2*sizeof(int), sizeof(int));
    d = m_getptr(descr_reg);
    op = &(td_operations(d)[opindex]);
    (*(op->op_unmarshall_op_call))(request+4*sizeof(int), &argv);
    r = FALSE;
    o = GetObjectDescr(id);
    assert(o != 0);
    mu_lock(&o->o_mutex);		/* lock object */

    if(o->o_owner != this_cpu) {
	/* Too late: object is not owned by this cpu anymore. */
fprintf(file, "%d: too late %d; now: %d\n", this_cpu, o->o_id, o->o_owner);
	r = TRYAGAIN;
    } else {
#ifdef OPTIMIZED
	assert(o->o_state);
	assert(o->o_state[this_cpu].o_replica);
#endif
	OBJ_RECEIVED_INC(o);
	while(!r) {
	    r = global_operation(o, 0, op, argv);
	    assert(r == TRUE || r == FALSE);
	    if(!r) {
		suspend_on(o, me);
		/*
		  put_suspend_rec(&op_trace, this_cpu, opindex, o, o->o_id,
		  me->process_id, REMOTE_OPER);
		  */
		mu_unlock(&o->o_mutex);	/* unlock object */
		go_to_sleep(me);
		if(o->o_owner != this_cpu) r = TRYAGAIN;
		mu_lock(&o->o_mutex);		/* lock object */
	    }
	}
    }
    /* marshall reply and free argb. */
    if(r == TRUE) {
	*replen = (*(op->op_size_op_return))(argv);
	*reply = m_malloc(*replen);
	(void) (*(op->op_marshall_op_return))(*reply, argv);
    } else {
	*reply = 0;
	*replen = 0;
        m_free(argv);
    }
    hdr->h_status = r;
    mu_unlock(&o->o_mutex);		/* done; unlock */
}


/* operation on a shared object located at obj->o_owner. */
static int
rpc_operation(process_p me, int *op_flags, t_object *obj, tp_dscr *d, int opindex, void **argtab)
{
    int r;
    op_dscr *op = &((td_operations(d))[opindex]);

    if (op) put_op_rec(&op_trace, this_cpu, op->op_func, obj, obj->o_id, me->process_id, RPC_OPER);
    mu_lock(&obj->o_mutex);		/* lock object */
    if(obj->o_owner == this_cpu) {	/* is it mine? */
	BSTINC(r_nlocal);
	OBJ_OWNER_INC(obj);
#ifdef OPTIMIZED
	assert(!obj->o_replicated);
	assert(obj->o_state);
	assert(obj->o_state[this_cpu].o_replica);
#endif
	while(obj->o_owner == this_cpu) {
	    if(global_operation(obj, 0, op, argtab)) {
	        if (op) put_operexit_rec(&op_trace, this_cpu, op->op_func, obj, obj->o_id,
			         me->process_id, RPC_OPER, 0);
    		mu_unlock(&obj->o_mutex);	/* done; unlock */
		return(TRUE);
	    } else {		/* guards failed; wait until obj changes */
		suspend_on(obj, me);
		if (op) put_suspend_rec(&op_trace, this_cpu, op->op_func, obj, obj->o_id,
				me->process_id, RPC_OPER);
    		mu_unlock(&obj->o_mutex);		/* unlock */
		go_to_sleep(me);
    		mu_lock(&obj->o_mutex);		/* lock */
	    }
	}
	r = TRYAGAIN;
    } else if(obj->o_owner >= 0 && obj->o_owner < ncpus) { 
	/* send the operation to the owner */
	assert(!obj->o_replicated);
	BSTINC(r_nremote);
	OBJ_REMOTE_INC(obj);
	r = RemoteOperation(me, obj, d, opindex, argtab);
	if (op) put_suspend_rec(&op_trace, this_cpu, op->op_func, obj, obj->o_id,
			me->process_id, RPC_OPER);
	if (op) put_operexit_rec(&op_trace, this_cpu, op->op_func, obj, obj->o_id,
			 me->process_id, RPC_OPER, 0);
    } else {
	assert(obj->o_owner == -1);
	assert(obj->o_replicated);
	r = TRYAGAIN;
    }
    mu_unlock(&obj->o_mutex);				/* done; unlock */
    return(r);
}


/* Perform an operation on an object. Code assumes that threads are 
 * scheduled non-preemptive.
 */
void
DoOperation(t_object *obj, int *op_flags, tp_dscr *d, int opindex, int attempted, void **argtab)
{
    int r;
    int old_owner;
    
    if (*op_flags & BLOCKING) {
	return;
    }
    old_owner = obj->o_owner;

#ifndef PROGRAM_MUST_SWITCH
#ifndef PREEMPTIVE
    if (! (*op_flags & NESTED)) {
	/* There are message waiting to be processed. Run the listener thread. */
	threadswitch();
    }
#endif
#endif
    if(o_isshared(obj) && !(*op_flags & NESTED)) {
        process_p me = GET_TASKDATA();
	do {
	    if(obj->o_replicated) {	/* obj is replicated on all cpus */
		r = brc_operation(me, op_flags, obj, d, opindex, argtab);
	    } else {		/* obj is located on cpu obj->o_owner */
		r = rpc_operation(me, op_flags, obj, d, opindex, argtab);
	    }
	} while (r == TRYAGAIN);
    } else {	/* local operation */
	BSTINC(oo_nlocal);
	local_operation(op_flags, obj, d, opindex, argtab);
    }
}

t_boolean
o_start_read(obj)
    register t_object *obj;
{
    if (! o_isshared(obj)) {
	BSTINC(oo_nlocal);
	return 1;
    }
    if (trace_on) return 0;
    if (! obj->o_replicated && obj->o_owner != this_cpu) return 0;
    mu_lock(&obj->o_mutex);
    if (obj->o_replicated) {
	BSTINC(oo_nread_trial);
	BSTINC(oo_nread);
	OBJ_READ_INC(obj);
	return 1;
    }
    if (obj->o_owner == this_cpu) {
	BSTINC(r_nlocal);
	OBJ_OWNER_INC(obj);
	OBJ_READ_INC(obj);
	return 1;
    }
    mu_unlock(&obj->o_mutex);
    return 0;
}

void
o_end_read(obj)
    register t_object *obj;
{
    if (o_isshared(obj)) {
	mu_unlock(&obj->o_mutex);
    }
#ifndef PROGRAM_MUST_SWITCH
#ifndef PREEMPTIVE
    threadswitch();
#endif
#endif
}

t_boolean
o_start_write(obj)
    register t_object *obj;
{
    if (! o_isshared(obj)) {
	BSTINC(oo_nlocal);
	return 1;
    }
    if (trace_on) return 0;
    if (! obj->o_replicated && obj->o_owner == this_cpu) {
	mu_lock(&obj->o_mutex);
	/* check again ... */
    	if (! obj->o_replicated && obj->o_owner == this_cpu) {
	    BSTINC(r_nlocal);
	    OBJ_OWNER_INC(obj);
	    return 1;
	}
	mu_unlock(&obj->o_mutex);
    }
    return 0;
}

void
o_end_write(obj, written)
    register t_object *obj;
{
    if (o_isshared(obj)) {
	if (written) {
		OBJ_WRITE_INC(obj);
		touched(obj);
	}
    	mu_unlock(&obj->o_mutex);
#ifndef PROGRAM_MUST_SWITCH
#ifndef PREEMPTIVE
	threadswitch();
#endif
#endif
    }
}
