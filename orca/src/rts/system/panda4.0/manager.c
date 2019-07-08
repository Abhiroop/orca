/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef MANAGER_INLINE
#define MANAGER_SRC
#endif

#include "interface.h"

#include "pan_sys.h"
#include "pan_align.h"
#include "pan_util.h"

#include "continuation.h"
#include "fork_exit.h"
#include "fragment.h"
#include "limits.h"
#include "manager.h"
#include "obj_tab.h"
#include "rts_comm.h"
extern unsigned long seqno;		/* hack: imported from rts_comm.c */
#include "rts_globals.h"
#include "rts_internals.h"
#include "rts_trace.h"

#ifdef MANAGER_SRC
#undef INLINE_FUNCTIONS
#include "msg_marshall.h"		/* break include cycle */
#endif
#include "inline.h"


#ifndef NEW
/*
 * We shouldn't be using these Ethernet heuristics on all
 * platforms!
 */
#define BCAST_COSTS(ncpus)    (2700+ncpus*7)   /* usec */
#define RPC_COSTS(ncpus)       2500            /* usec */

#else

/* New heuristic: weigh by the number of executed message handlers
 * Note: this heuristic fails for the compiler estimates of TSP on
 * large number of processors :-(
 */
#define BCAST_COSTS(ncpus)    (2 + (ncpus)-1)	/* includes fetching of seqno */
#define RPC_COSTS(ncpus)      2

#endif

#define max(a,b)	((a) > (b) ? (a) : (b))

float man_bcast_costs;
float man_rpc_costs;

#ifdef OUTLINE		/* { */

typedef struct replicate_hdr {
    int sender;
    int oid;
    rc_msg_p	msg;
} replicate_hdr_t;

typedef struct migrate_hdr {
    int sender;
    int max_location;
    int oid;
    rc_msg_p msg;
} migrate_hdr_t;

typedef struct delete_hdr {
    float writes;
    float reads;
    int   sender;
    int   oid;
} delete_hdr_t;

struct migr_cont {
    int src;
    fragment_p obj;
};

struct mcast_cont {
    int handle;
    pan_iovec_p	iov;
    int		iov_size;
    void       *proto;
    int		proto_size;
};


static int r_replicate(int op, int upcall, pan_msg_p request, void *proto);
static int r_migrate(int op, int upcall, pan_msg_p request, void *proto);
static int r_fetch_obj(int op, int upcall, pan_msg_p request, void *proto);
static int r_delete_ref(int op, int upcall, pan_msg_p request, void *proto);

static int replicate_handle;
static int exclusive_handle;
static int migrate_handle;
static int fetch_handle;
static int delete_handle;

static int man_delete_proto_top;
static int man_delete_proto_start;
#define man_delete_hdr(p)	((int *)((char *) (p) + man_delete_proto_start))
static int man_migrate_proto_top;
static int man_migrate_proto_start;
#define man_migrate_hdr(p)	((migrate_hdr_t *)((char *) (p) + man_migrate_proto_start))
static int man_replicate_proto_top;
static int man_replicate_proto_start;
#define man_replicate_hdr(p)	((replicate_hdr_t *)((char *) (p) + man_replicate_proto_start))

#define how( obj)    ((obj->fr_flags & MAN_LOCAL_ONLY) ? "local" : "bcast")

static int rpc_ready, grp_ready;
static int man_alive = 0;


/************/

static int
man_mcast_cont(void *state)
{
    struct mcast_cont *mc = state;

    /*
     * Koen : ad hoc code to avoid sending out group messages when the
     * RTS has already shut down the Panda layer because all Orca
     * processes terminated. (Between creating a continuation and
     * handling it by the cont_immediate_worker thread, other exit
     * messages can be processed.)
     */
    if (man_alive) {
	rc_mcast(mc->handle, mc->iov, mc->iov_size, mc->proto, mc->proto_size);
    }
else printf( "%d) gotcha\n", rts_my_pid);
    return CONT_NEXT;
}


static void
man_mcast(int handle, pan_iovec_p iov, int iov_size, void *proto,
	  int proto_size)
{
    struct mcast_cont *mc;

    assert(!rts_trylock());

    /*
     * Build a continuation and hand it off to the worker thread.
     */
    mc = cont_alloc(&cont_immediate_queue, sizeof(*mc), man_mcast_cont);
    mc->handle = handle;
    mc->iov        = iov;
    mc->iov_size   = iov_size;
    mc->proto      = proto;
    mc->proto_size = proto_size;
    cont_save(mc, 0);
}

/************/

void
man_start(void)
{
    /*
     * Make sure manager flag does not clash with other object
     * flags.
     */
    assert( (MAN_FLAGS & RO_MASK) == 0);    

    man_alive = 1;
    rpc_ready = 0;
    grp_ready = 0;

    replicate_handle = rc_export(r_replicate);
    exclusive_handle = rc_export(r_migrate);
    migrate_handle   = rc_export(r_migrate);
    fetch_handle     = rc_export(r_fetch_obj);
    delete_handle    = rc_export(r_delete_ref);

    man_delete_proto_start = align_to(rts_mcast_proto_top, int);
    man_delete_proto_top = man_delete_proto_start + sizeof(int);
    if (rts_mcast_proto_top > rts_rpc_proto_top) {
        man_migrate_proto_start = align_to(rts_mcast_proto_top, migrate_hdr_t);
    }
    else {
        man_migrate_proto_start = align_to(rts_rpc_proto_top, migrate_hdr_t);
    }
    man_migrate_proto_top = man_migrate_proto_start + sizeof(migrate_hdr_t);
    man_replicate_proto_start = align_to(rts_mcast_proto_top, replicate_hdr_t);
    man_replicate_proto_top = man_replicate_proto_start + sizeof(replicate_hdr_t);
    man_bcast_costs = BCAST_COSTS(rts_nr_platforms);
    man_rpc_costs   = RPC_COSTS(rts_nr_platforms);
    if (verbose && rts_my_pid == 0) {
	printf("man_bcast_costs = %g, man_rpc_costs = %g\n", man_bcast_costs, man_rpc_costs);
    }
}


void
man_end(void)
{
    man_alive = 0;
}


void
man_init(manager_p man, float reads, float writes)
{
    float access_sum = reads+writes;

    man->fixed_strategy = 0;

    man->compiler.write_sum = writes;
    man->compiler.access_sum = access_sum;
    man->compiler.max_access = access_sum;
    man->compiler.max_location = rts_my_pid;
    man->compiler.cache_bcast = writes * man_bcast_costs;
    man->compiler.cache_rpc   = 0;

#ifndef NDYNAMIC
    man->runtime.write_sum = 0;
    man->runtime.access_sum = 0;
    man->runtime.max_access = 0;
    man->runtime.max_location = rts_my_pid;
    man->runtime.delay = 3;
    man->runtime.hist_bcast = 0;
    man->runtime.prev_bcast = 0;
    man->runtime.hist_rpc   = 0;
    man->runtime.prev_rpc   = 0;
#endif
}


void
man_clear(manager_p man)
{
}

void
man_kill_object(fragment_p obj)
/*
 * If the system-wide reference count has dropped to zero, the object
 * can safely be removed from the object-table and deallocated.
 */
{
    int saved_id = obj->fr_oid;    /* o_kill_rtsdep invalidates obj */

#ifdef RTS_VERBOSE
    /* printf( "%d: delete object %s\n", rts_my_pid, obj->fr_name); */
#endif

    assert(obj->fr_total_refs == 0);

    if (obj->fr_manager != NULL) {
	man_clear( obj->fr_manager);
	m_free(obj->fr_manager);
	obj->fr_manager = NULL;
    }

    if (obj->fr_fields != NULL) {
	if ( obj->fr_flags & MAN_VALID_FIELD) {
	    if (td_objinfo(obj->fr_type)->obj_rec_free) {
		(*(td_objinfo(obj->fr_type)->obj_rec_free))(obj->fr_fields);
	    }
	}
	m_free(obj->fr_fields);
	obj->fr_fields = NULL;
    }

    o_kill_rtsdep(obj);        /* destroy RTS info */

    otab_remove(saved_id);     /* remove from obj. table (if present) */
}

void
man_strategy( fragment_p obj, int replicated, int cpu)
{
    if (ignore_strategy) {
	return;
    }

    /*
     * Hack for dynamic arrays; __Score() is called before
     * the user has had a chance to initialize the array.
     * Call __Score() again with the initialized object.
     */
    if (f_get_status( obj) == f_unshared)
	rts_score( obj, obj->fr_type, (double) 0, (double) 0, (double) 481,
		   DONT_LOCK_RTS);

    if (replicated) {
	obj->fr_flags |= RO_REPLICATED;
	cpu = rts_base_pid;
    } else if ( cpu != rts_my_pid) {
	obj->fr_owner = cpu;
	obj->fr_flags &= ~RO_MANAGER;
    }
    obj->fr_manager->fixed_strategy = 1;
    /*
     * We abuse DoFork to place the object as desired.
     * Rts_proc_descr is a special process descriptor,
     * which is treated specially by DoFork.
     */
    rts_unlock();
    DoFork(cpu - rts_base_pid, &rts_proc_descr, (void **)&obj);
    rts_lock();
}


/***** marshalling code *****/

int
man_nbytes(fragment_p obj)
{
    return sizeof(manager_t);   /* macro? */
}

char *
man_marshall( char *buf, fragment_p obj)
{
    (void)memmove(buf, obj->fr_manager, sizeof(manager_t));
    man_clear( obj->fr_manager);
    m_free( obj->fr_manager);
    obj->fr_manager = 0;
    return( buf + sizeof( manager_t));
}

void
man_unmarshall( pan_msg_p m, fragment_p obj)
{
    pan_msg_consume(m, obj->fr_manager,sizeof(manager_t));
}



/*
 * process creation/termination of object references; locking by callee
 */
void
man_inc(fragment_p obj, int src, int dst, float reads, float writes)
{
    float access_sum;
    float cpu_access;
    manager_p man;
    cont_queue_p q;

    assert(!rts_trylock());

    cpu_access = access_sum = reads + writes;

    if (dst == rts_my_pid) {
	cpu_access = (obj->fr_info.access_sum += access_sum);
    }

    obj->fr_total_refs++;

    if (f_get_status(obj) & RO_MANAGER) {
	float bcast, rpc;

	assert( obj->fr_manager);
	man = obj->fr_manager;
	/* Don't compute new compiler scores from totals, since the scores
	 * so far have been aged. Add "new" costs to aged old costs.
	 */
	bcast = max(man->compiler.write_sum, 1) * man_bcast_costs;
	rpc = max(man->compiler.access_sum - man->compiler.max_access, 1)
					* man_rpc_costs;

	man->compiler.write_sum += writes;
	man->compiler.access_sum += access_sum;
	if (cpu_access > man->compiler.max_access) {
	    man->compiler.max_access = cpu_access;
	    man->compiler.max_location = dst;
	}
	/* Make sure costs will not be zero, so man_take_decision can make
	 * a proper trade-off between static and dynamic costs. (A zero
	 * static cost allways overrides a dynamic cost)
	 */
	man->compiler.cache_bcast
		/* = max(man->compiler.write_sum, 1) * man_bcast_costs; */
		+= max(man->compiler.write_sum, 1) * man_bcast_costs - bcast;
	man->compiler.cache_rpc
		/* = max(man->compiler.access_sum - man->compiler.max_access, 1)
					* man_rpc_costs; */
		+= max(man->compiler.access_sum - man->compiler.max_access, 1)
					* man_rpc_costs - rpc;
	man_take_decision( obj);
    }

    /*
     * Help! New Orca process fetches data of shared object object but
     * "forgets" to fetch continuations (guarded operations) as well.
     *
     * Solution/hack: flush continuation queue by temporarily setting
     * status to illegal. All guarded operations will be broadcast
     * again.
     */
    q = f_get_queue(obj);
    if ( (f_get_status(obj) & RO_REPLICATED) && cont_pending(q) ) {
	f_status_t stat = f_get_status(obj);

	obj->fr_flags &= ~RO_MASK;
	obj->fr_flags |= F_TEMP_ILL;
	cont_resume(q);			/* Flush queue. */
	assert( !cont_pending(q));	/* Is it really empty? */
	obj->fr_flags &= ~RO_MASK;
	obj->fr_flags |= stat;
    }

    rts_man_check(obj);
}


void
man_dec( fragment_p obj, int cpu, float reads, float writes)
{
    manager_p man;
    float access_sum = reads+writes;

    assert(!rts_trylock());

    obj->fr_total_refs--;
    if (cpu == rts_my_pid) {
	obj->fr_info.access_sum -= access_sum;
    }
    if ( f_get_status(obj) & RO_MANAGER) {
	assert( obj->fr_manager);
	man = obj->fr_manager;
	man->compiler.write_sum -= writes;
	man->compiler.access_sum -= access_sum;
	if ( cpu == man->compiler.max_location) {
	    if ( cpu == rts_my_pid) {
		man->compiler.max_access -= access_sum;
	    } else {
			/* by lack of better info, take manager self */
		man->compiler.max_access = obj->fr_info.access_sum;
		man->compiler.max_location = rts_my_pid;
	    }
	    /* No need to update runtime info! */
	}
	/* Don't compute new compiler scores from totals, since the scores
	 * so far have been aged. No clue how to substract aged costs, so
	 * just don't do it since they might be zero anyway.
	 *
	 * man->compiler.cache_bcast = man->compiler.write_sum * man_bcast_costs;
	 * man->compiler.cache_rpc
	 *	= max(man->compiler.access_sum - man->compiler.max_access, 1)
						* man_rpc_costs;
	 */
	/*
	 * We don't do this to avoid unnecessary moves on program
	 * termination.
	 *
	 * if ( man->glob_ref>0) {
	 *     man_take_decision( obj); 
	 * }
	 */
    }
    rts_man_check(obj);
}


void
man_delete( fragment_p objects, float reads, float writes, int nr)
{
    int i;
    int do_delete = 0;
    pan_iovec_p iov, iovp;
    int *n_delete;
    delete_hdr_t *hdr;

    for ( i=0; i< nr; i++) {
	fragment_p f = &objects[i];
	
	if ( f_get_status(f) != f_unshared) {
	    if ( (f->fr_flags & MAN_LOCAL_ONLY) == 0) {
		do_delete++;
	    }
	}
    }
    iovp = iov = m_malloc(do_delete * sizeof(pan_iovec_t));
    for ( i=0; i< nr; i++) {
	fragment_p f = &objects[i];

	/* Note that it is possible with arrays of (shared) objects that
	 * Score() is never called, so objects do not have to be removed
	 * from the object table.
	 */
	if ( f_get_status(f) != f_unshared) {
	    if ( (f->fr_flags & MAN_LOCAL_ONLY) == 0) {
		iovp->data = hdr = m_malloc(sizeof(delete_hdr_t));
		iovp->len = sizeof(delete_hdr_t);
		hdr->writes = writes;
		hdr->reads  = reads;
		hdr->sender = rts_my_pid;
		hdr->oid    = f->fr_oid;
		iovp++;
	    } else {
		/*
		 * No one else knows this object, so we do not send
		 * a 'delete' message.
		 */
		man_dec(f, rts_my_pid, reads, writes);
	    }
	    f->o_rtsdep = 0;		/* fool o_free() */
	}
    }
    if ( do_delete > 0) {
	/* no man_mcast() because this message has to be sent out before
	 * the exit message!
	 */
        void *proto = rc_proto_create();
	n_delete = man_delete_hdr(proto);
	*n_delete = do_delete;
	rc_mcast(delete_handle, iov, do_delete, proto, man_delete_proto_top);
    }
}

static void
print_decision(fragment_p obj, int location)
{
	manager_p man = obj->fr_manager;
	char *decision;

	if (location >= 0) {
	    if (f_get_status(obj) & RO_REPLICATED) {
		decision = "exclusive at";
	    }
	    else decision = "migrated to";

	    printf( "[t:%3d] %2d) %s decision: %s %s %d (W=%d, A=%d, M=%d)\n",
		  seqno, rts_my_pid, how(obj), obj->fr_name, decision, location,
		  man->runtime.write_sum, man->runtime.access_sum,
		  man->runtime.max_access);
	}
	else {

		printf( "[t:%3d] %2d) %s decision: %s replicate (W=%d, A=%d, M=%d)\n",
			  seqno, rts_my_pid, how(obj), obj->fr_name,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
	}
	if (verbose > 1) {
	    printf("compiler: ws: %d, as: %d, ma: %d, ml: %d, cb: %g, cr: %g\n",
		man->compiler.write_sum,
		man->compiler.access_sum,
		man->compiler.max_access,
		man->compiler.max_location,
		man->compiler.cache_bcast,
		man->compiler.cache_rpc);
	    printf("runtime: ws: %d, as: %d, ma: %d, ml: %d, de: %d, hb: %g, pb: %g, hr: %g, pr: %g\n",
		man->runtime.write_sum,
		man->runtime.access_sum,
		man->runtime.max_access,
		man->runtime.max_location,
		man->runtime.delay,
		man->runtime.hist_bcast,
		man->runtime.prev_bcast,
		man->runtime.hist_rpc,
		man->runtime.prev_rpc);
	}
}

void
man_take_decision( fragment_p obj)
/*
 * The flag RO_MANAGER indicates if this nodes acts as the manager or not.
 * The field fr_manager indicates if this node has the manager info or not.
 * Note that 'fr_manager' can be set while the flag RO_MANAGER is off; this
 * happens during migration of the manager.
 */
{
    manager_p man = obj->fr_manager;
    float bcast, rpc;
    int location;
    float rt_bcast, rt_rpc, diff;
    int rt_location;

    assert(!rts_trylock());

    if ( (obj->fr_flags & MAN_FUZZY_STATE) || man->fixed_strategy) {
	return;
    }

    assert(obj->fr_owner == rts_my_pid);

#ifndef NDYNAMIC
    /*
     * Local info is never piggybacked, so process it now to avoid
     * taking a wrong decision. 
     *
     * Avoid moving some (barrier) object around in an iterative
     * application by updating the maximum only if the new value
     * exceeds the current best by a fraction access_sum/#cpus.
     * equation: max_access > access_sum/#cpu + obj.nr_accesses
     */
    if (obj->fr_info.nr_accesses >= man->runtime.max_access) {
	man->runtime.max_access = obj->fr_info.nr_accesses;
	man->runtime.max_location = rts_my_pid;
    } else if ((man->runtime.max_access - obj->fr_info.nr_accesses)*rts_nr_platforms < man->runtime.access_sum) {
	man->runtime.max_location = rts_my_pid;
    }
#endif

    /*
     * Compute costs of both object states (replicated or not) for two
     * heuristics (compiler vs. rts).
     */
    bcast = man->compiler.cache_bcast;
    rpc   = man->compiler.cache_rpc;
    location = man->compiler.max_location;

#ifndef NDYNAMIC
    /* 
     * If compiler info and rts accounting should be combined, a
     * conflict can occur. Either both methods disagree on whether
     * or not to replicate an object, or they disagree about the
     * location of an object in single copy mode. We take the decision
     * of the heuristic that shows the largest difference between the
     * costs for replicating and single copy mode.
     */
    if ( use_runtime_info) {
	/*
	 * Grr, problems with overflow, so immediately convert to double.
	 */
	rt_bcast = man->runtime.write_sum * man_bcast_costs;
	rt_rpc   = (man->runtime.access_sum - man->runtime.max_access)
							* man_rpc_costs;
	rt_location = man->runtime.max_location;

	/* For adaptive behaviour use time sensitive values instead of
	 * absolute sums.
	 */
	diff = rt_bcast - man->runtime.prev_bcast;
	if (diff < 0) diff = 0;		/* costs must not be negative */
	man->runtime.prev_bcast = rt_bcast;
	rt_bcast = (3*man->runtime.hist_bcast + diff) / 4;
	man->runtime.hist_bcast = rt_bcast;

	diff = rt_rpc - man->runtime.prev_rpc;
	if (diff < 0) diff = 0;		/* costs must not be negative */
	man->runtime.prev_rpc = rt_rpc;
	rt_rpc = (3*man->runtime.hist_rpc + diff) / 4;
	man->runtime.hist_rpc = rt_rpc;

	if ( !use_compiler_info) {
	    bcast = rt_bcast;
	    rpc = rt_rpc;
	    location = rt_location;

	/* Only use runtime info if enough statistics are available.
	 * Wait at least until "everybody" has accessed the object once.
	 * Two different cases: replicated and single copy mode
	 */
	} else if (man->runtime.access_sum >= man->runtime.max_access
				+ (bcast<rpc ? obj->fr_total_refs-1 : 1)) {
	    /* Conflicts between compiler estimates and runtime statistics are
	     * resolved by summing the scores => strongest opinion wins
	     */
	    bcast += rt_bcast;
	    rpc += rt_rpc;
	    location = rt_location;

	    /* The compiler estimate must only be used initially, so decay
	     * the estimate
	     */
    	    man->compiler.cache_bcast /= 2;
    	    man->compiler.cache_rpc /= 2;
	}
    }
#endif

    /* Use a threshold to avoid trashing between exclusive and replicated.
     * Also, make sure going from replicated to exclusive is tougher because
     * in replicated mode, the manager is not informed about "remote" reads
     * until the remote processor broadcasts a write operation.
     */

    if ( f_get_status(obj) & RO_REPLICATED) {
	if ( rpc < ((float)0.8)*bcast && !replicate_all) {
#ifdef TRACING
	    char buf[256];

	    sprintf( buf, "%80s", " ");
	    sprintf( buf, "%s: %s exclusive at %d (W=%d, A=%d, M=%d)",
			  how(obj), obj->fr_name, location,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
	    trc_event( man_decision, buf);
#endif
	    if (verbose) 
		print_decision(obj, location);
	    /* only broadcast decision for external refs */
	    if ( (obj->fr_flags & MAN_LOCAL_ONLY) == 0) {
		rc_msg_p	msg = rc_msg_create();
		migrate_hdr_t  *hdr;

		msg->proto = rc_proto_create();
		hdr = man_migrate_hdr(msg->proto);	/* sizeof(migrate_hdr_t)); */
		msg->iov = 0;
		hdr->sender = rts_my_pid;
		hdr->max_location = location;
		hdr->oid = obj->fr_oid;
		hdr->msg = msg;
		obj->fr_flags |= MAN_FUZZY_STATE;
		man_mcast(exclusive_handle, 0, 0,
			  msg->proto, man_migrate_proto_top);

		/*
		 * Change status on receipt of BC message.
		 * Any decisions made before changing state
		 * to EXCLUSIVE will be ignored.
		 */
	    } else {
		/* change status immediately */
#ifdef TRACING
		char buf[256];

		sprintf( buf, "%80s", " ");
		sprintf( buf, "change status: %s exclusive at %d",
			      obj->fr_name, location);
		trc_event( man_change, buf);
#endif
		obj->fr_owner = location;
		obj->fr_flags &= ~RO_REPLICATED;
		if ( obj->fr_owner != rts_my_pid) {
		    obj->fr_flags &= ~RO_MANAGER;
		    /* object will be fetched by forkee */
		}
		cont_resume(f_get_queue(obj));
	    }
	}
    } else {
	if (!replicate_none && (bcast < ((float)0.9)*rpc || replicate_all)) {
#ifdef TRACING
	    char buf[256];

	    sprintf( buf, "%80s", " ");
	    sprintf( buf, "%s: %s replicate (W=%d, A=%d, M=%d)",
			  how(obj), obj->fr_name,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
	    trc_event( man_decision, buf);
#endif
	    if (verbose) 
		print_decision(obj, -1);
	    obj->fr_flags |= RO_REPLICATED;
	    /* only broadcast decision for external refs */
	    if ( (obj->fr_flags & MAN_LOCAL_ONLY) == 0) {
		replicate_hdr_t *hdr;
		rc_msg_p	msg;

		msg = rc_msg_create();
		msg->proto_size = man_replicate_proto_top;
		msg->proto = rc_proto_create();
		msg->iov = mm_get_iovec(0);
		msg->iov_size = 0;

		mm_pack_sh_object(&msg->iov, &msg->iov_size, obj);
		hdr = man_replicate_hdr(msg->proto);	/* sizeof(replicate_hdr_t)); */
		hdr->sender = rts_my_pid;
		hdr->oid    = obj->fr_oid;
		hdr->msg    = msg;

		/* Problem: hand replicate message to low priority thread,
		 * Orca process then broadcast a write operation, which gets
		 * out of the door before the replicate message => update is
		 * lost on all machines (except locally)
		 */
		obj->fr_flags &= ~RO_MASK;	 /* invalid until BC is back */
		obj->fr_flags |= F_IN_TRANSIT;

		man_mcast(replicate_handle, msg->iov, msg->iov_size,
			  msg->proto, man_replicate_proto_top);
	    } else {
#ifdef TRACING
		char buf[256];

		sprintf( buf, "%80s", " ");
		sprintf( buf, "change status: %s replicated", obj->fr_name);
		trc_event( man_change, buf);
#endif
	    }
	    /* Moved after if statement so that the state is set to
	     * F_IN_TRANSIT. This avoids that a local Orca process can put a
	     * write guard on the object before the replicate message is send.
	     * (The problem is that cont_resume() may release the lock to send
	     * a failed message, so a local process can get at the object.)
	     */
	    cont_resume(f_get_queue(obj));
	} else if (location != obj->fr_owner) {	/* NO threshold?! :-( */
#ifdef TRACING
	    char buf[256];

	    sprintf( buf, "%80s", " ");
	    sprintf( buf, "%s: %s migrate to %d (W=%d, A=%d, M=%d)",
			  how(obj), obj->fr_name, location,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
	    trc_event( man_decision, buf);
#endif
	    if (verbose) 
		print_decision(obj, location);
	    /* only broadcast decision for external refs */
	    if ( (obj->fr_flags & MAN_LOCAL_ONLY) == 0) {
		migrate_hdr_t  *hdr;
		rc_msg_p	msg;

		msg = rc_msg_create();
		msg->proto_size = man_migrate_proto_top;
		msg->proto = rc_proto_create();
		msg->iov = 0;
		msg->iov_size = 0;
		hdr = man_migrate_hdr(msg->proto);	/* sizeof(migrate_hdr_t)); */
		hdr->sender = rts_my_pid;
		hdr->max_location = location;
		hdr->oid = obj->fr_oid;
		hdr->msg = msg;

		obj->fr_flags |= MAN_FUZZY_STATE;
		man_mcast(migrate_handle, 0, 0, msg->proto, man_migrate_proto_top);
	    } else {
#ifdef TRACING
		char buf[256];

		sprintf( buf, "%80s", " ");
		sprintf( buf, "change status: %s exclusive at %d",
			      obj->fr_name, location);
		trc_event( man_change, buf);
#endif
		obj->fr_owner = location;
		obj->fr_flags &= ~RO_MANAGER;
		cont_resume(f_get_queue(obj));
	    }
	}
    }
}


/***** service routines *****/


static int
r_replicate(int handle, int upcall, pan_msg_p request, void *proto)
{
    replicate_hdr_t *hdr;
    fragment_p obj;

    assert(handle == replicate_handle);

    hdr = man_replicate_hdr(proto);	/* sizeof(replicate_hdr_t)); */

    if ((obj = otab_lookup(hdr->oid))) {
#ifdef TRACING
	char buf[256];

	sprintf( buf, "%80s", " ");
	sprintf( buf, "change status: %s replicated", obj->fr_name);
	trc_event( man_change, buf);
#endif

	/* assert( !(obj->fr_flags & RO_REPLICATED)); */
	/* object has been reused, while manager thought it had died
	 * and would be restored => inconsistency; bloody optimisations
	 */

	/* Feature, reference count of object is always updated by
	 * marshalling code. However, between taking the decision to
	 * replicate the object and arrival of this message, some FORK
	 * messages might have been processed. In that case the ref
	 * count included in the message is too low and should be
	 * neglected!
	 */
	if (hdr->sender == rts_my_pid) {
	    assert( f_get_status(obj) == f_in_transit);
	    /* Clear message before resetting object flags. If I understand
	       correctly, we must prevent the object data from getting
	       touched before the msg is cleared (Ceriel).
	    */
	    rc_proto_clear(hdr->msg->proto);
	    rc_msg_clear(hdr->msg);
	    obj->fr_flags &= ~RO_MASK;
	    obj->fr_flags |= F_MANAGER;
	    cont_resume(f_get_queue(obj));
	} else {
	    rts_unlock();
	    mm_unpack_sh_object(request, obj);
	    rts_lock();
	    assert( f_get_status(obj) == f_replicated);
	}
	rts_man_check(obj);
    }

    else if (hdr->sender == rts_my_pid) {
	rc_proto_clear(hdr->msg->proto);
	rc_msg_clear(hdr->msg);
    }
    rc_mcast_done();
    return 1;
}


static int
r_migr_cont(void *state)
{
    struct migr_cont *mc = state;
    fragment_p obj = mc->obj;
    int src = mc->src;
    void       *proto;
    pan_msg_p	rep;
    void       *reply_proto;
    migrate_hdr_t	*hdr;

    assert(!rts_trylock());

    proto = rc_proto_create();
    hdr   = man_migrate_hdr(proto);
    hdr->oid = obj->fr_oid;
    hdr->sender = rts_my_pid;

#ifdef RTS_VERBOSE
    printf("%d) r_migr_cont: fetching remote object\n", rts_my_pid);
#endif
    /*
     * fetch object data
     */
    rc_rpc(src, fetch_handle, 0, 0, proto, man_migrate_proto_top,
    	   &rep, &reply_proto);

    rts_unlock();
    mm_unpack_sh_object(rep, obj);
    pan_msg_clear(rep);
    rts_lock();

#ifdef RTS_VERBOSE
    printf("%d) r_migr_cont: got remote object\n", rts_my_pid);
#endif

    assert(obj->fr_flags & RO_MANAGER); /* I should be manager now */

#ifdef TRACING
    {
	char buf[256];

	sprintf( buf, "%80s", " ");
	sprintf( buf, "change status: %s exclusive at %d",
		      obj->fr_name, rts_my_pid);
	trc_event( man_change, buf);
    }
#endif

    /*
     * manager transfer is completed at all sites
     */
    obj->fr_flags &= ~MAN_FUZZY_STATE;
    obj->fr_flags &= ~RO_REPLICATED;
    obj->fr_owner = rts_my_pid;
    cont_resume(f_get_queue(obj));

    /*
     * Something might have changed between taking the decision to
     * migrate the object and the actual data transfer.
     */
    man_take_decision( obj);
    rts_man_check(obj);

    rc_proto_clear(proto);

    rc_mcast_done();
    return CONT_NEXT;
}


static int
r_migrate(int handle, int dummy, pan_msg_p request, void *proto)
{
    fragment_p obj;
    int src, cpu, mc_done = 1;
    migrate_hdr_t *hdr;
    struct migr_cont *mc;

    assert(handle == migrate_handle || handle == exclusive_handle);

    hdr = man_migrate_hdr(proto);		/* sizeof(migrate_hdr_t)); */

    src = hdr->sender;

    if ((obj = otab_lookup(hdr->oid))) { /* do we know this object at all? */

	cpu = hdr->max_location;
        if (src == rts_my_pid) {
	    rc_proto_clear(hdr->msg->proto);
	    rc_msg_clear(hdr->msg);
        }
	if (cpu == rts_my_pid && 
	    src != rts_my_pid) { /* object is put on this node */
	    /*
	     * Build a continuation and hand it off to the worker thread.
	     */
	    mc = cont_alloc(&cont_immediate_queue, sizeof(*mc), r_migr_cont);
	    mc->src = src;
	    mc->obj = obj;
	    cont_save(mc, 0);
	    mc_done = 0;
#ifdef RTS_VERBOSE
	    printf("%d) r_migrate: built continuation\n", rts_my_pid);
#endif
	} else {

#ifdef TRACING
	    {
		char buf[256];

		sprintf( buf, "%80s", " ");
		sprintf( buf, "change status: %s exclusive at %d",
			      obj->fr_name, cpu);
		trc_event( man_change, buf);
	    }
#endif

	    /*
	     * Manager transfer is completed at all sites.
	     */
	    obj->fr_flags &= ~MAN_FUZZY_STATE;
	    obj->fr_flags &= ~RO_REPLICATED;
	    obj->fr_owner = cpu;
	    if (cpu != rts_my_pid) {
		obj->fr_flags &= ~RO_MANAGER;
	    } else {
		/*
		 * Something might have changed between taking the
		 * decision to migrate the object and the actual
		 * data transfer.
		 */
		assert( obj->fr_flags & RO_MANAGER);
		man_take_decision( obj);
	    }
	    cont_resume(f_get_queue(obj));

	    rts_man_check(obj);

	    /* 
	     * Am i the source, but not destination, of the object?
	     */
	    if (src == rts_my_pid && cpu != rts_my_pid) {
		if (!rpc_ready) { /* group listener first, "wait" for RPC */
		    grp_ready++;
		    mc_done = 0;
		} else {
		    rpc_ready = 0;
		}
	    }
	}
    }
    else if (src == rts_my_pid) {
	rc_proto_clear(hdr->msg->proto);
	rc_msg_clear(hdr->msg);
    }
    if (mc_done) {
	rc_mcast_done();
    }	

    return 1;
}

void
clean_r_fetchobj(rc_msg_p msg)
{
    int i = 0;
    int mc_done;

    while (i < msg->iov_size) {
        i = mm_clean_sh_msg(msg->iov, i);
    }
    rts_lock();
    mm_free_iovec(msg->iov);
    rc_proto_clear(msg->proto);
 
    /*
     * Synchronize with r_migrate().
     */
    if (grp_ready) {	/* r_migrate went first, _now_ it is done */
	grp_ready = 0;
	mc_done = 1;
    } else {		/* I got here first, r_migrate can proceed */
	rpc_ready++;
	mc_done = 0;
    }

    if (mc_done) {
	rc_mcast_done();
    }
    rts_unlock();
    rc_msg_clear(msg);
}

static int
r_fetch_obj(int handle, int upcall, pan_msg_p request, void *proto)
{
    fragment_p obj;
    rc_msg_p	reply;
    migrate_hdr_t	*hdr;

    assert(handle == fetch_handle);
    hdr = man_migrate_hdr(proto);

    obj = otab_lookup(hdr->oid);
    assert(obj);
#ifdef RTS_VERBOSE
    printf("%d: r_fetch_obj: returning obj %s, fields = %p\n",
	   rts_my_pid, obj->fr_name, obj->fr_fields);
#endif

    reply = rc_msg_create();
    /*
     * problem: RPC might arrive before issuing BC is handled locally,
     *          so owner field is still set to this node and the manager
     *          state won't be marshalled.
     */
    obj->fr_owner = hdr->sender;
    reply->proto = rc_proto_create();
    reply->proto_size = rts_reply_proto_top;
    reply->iov = mm_get_iovec(0);
    reply->iov_size = 0;
    mm_pack_sh_object(&reply->iov, &reply->iov_size, obj);
    reply->clearfunc = clean_r_fetchobj;
    assert(obj->fr_manager == NULL);
    rts_man_check(obj);

    rc_untagged_reply(upcall, reply);

    return 1;
}


static int
r_delete_ref(int handle, int dummy, pan_msg_p request, void *proto)
/*
 * Needed to adjust reference count for a process's "local" objects
 * that have become shared. A process's parameters are dealt with by
 * r_exit().
 */
{
    fragment_p obj;
    delete_hdr_t *hdr;
    int *n_delete;
    int do_delete;
    int i;

    assert(handle == delete_handle);

    n_delete = man_delete_hdr(proto);
    do_delete = *n_delete;
    hdr = m_malloc(do_delete * sizeof(delete_hdr_t));
    rts_unlock();
    pan_msg_consume(request, hdr, do_delete * sizeof(delete_hdr_t));
    rts_lock();
    for ( i=0; i<do_delete; i++) {
	if ((obj = otab_lookup(hdr[i].oid))) {
	    man_dec(obj, hdr[i].sender, hdr[i].reads, hdr[i].writes);
	}
    }
    m_free(hdr);
    rc_mcast_done();
    return 1;
}

#endif		/* } */

INLINE void
man_get_piggy_info( fragment_p obj, man_piggy_p info)
{
#ifndef NDYNAMIC
    *info = obj->fr_info;
    obj->fr_info.delta_reads = 0;
#endif
}

INLINE void
man_tick( fragment_p obj, int modified)
{
#ifndef NDYNAMIC
    if (obj->fr_manager != NULL) {
	obj->fr_manager->runtime.access_sum++;
	if (modified) {
	    obj->fr_manager->runtime.write_sum++;
	}
    }
#endif
}

INLINE void
man_op_finished( fragment_p obj)
{
#ifndef NDYNAMIC
    if (obj->fr_manager != NULL && obj->fr_manager->runtime.delay-- <= 0) {
	man_take_decision( obj);
	obj->fr_manager->runtime.delay = 10;
    }   
#endif
}

/*
 * Process piggy back info; RTS should be locked.
 */
INLINE void
man_process_piggy_info( man_piggy_p piggy, fragment_p obj, int src_cpu)
{
#ifndef NDYNAMIC
    if ( f_get_status(obj) & RO_MANAGER) {
	manager_p man = obj->fr_manager;

	/*
	 * Process piggybacked info from outside.
	 */
	if (piggy->access_sum > man->compiler.max_access) {
/* Commented out by Ceriel: I think that this is no longer correct because of
   the aging of compiler info.
	    man->compiler.cache_rpc
		= max(man->compiler.access_sum - man->compiler.max_access, 1)
						* man_rpc_costs;
   Replaced by the following two lines (which may be wrong as well ...
*/
	    man->compiler.cache_rpc -= (piggy->access_sum - man->compiler.max_access) * man_rpc_costs;
	    if (man->compiler.cache_rpc <= 0) man->compiler.cache_rpc = man_rpc_costs;

	    man->compiler.max_access   = piggy->access_sum;
	    man->compiler.max_location = src_cpu;
	}
	/* Avoid migrating an object to a cpu that is only 1 access ahead
	 */
	if (piggy->nr_accesses > man->runtime.max_access+1) {
	    man->runtime.max_location = src_cpu;
	    man->runtime.max_access = piggy->nr_accesses;
	}
	man->runtime.access_sum += piggy->delta_reads;
    }
#endif
}

#undef MANAGER_SRC
