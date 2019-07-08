/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#include "interface.h"
#include "pan_sys.h"
#include "pan_util.h"
#include "continuation.h"
#include "fork_exit.h"
#include "fragment.h"
#include "manager.h"
#include "msg_marshall.h"
#include "rts_comm.h"
#include "obj_tab.h"
#include "limits.h"
#include "rts_trace.h"
#include "rts_globals.h"


#define BCAST_COSTS(ncpus)    (2700+ncpus*7)      /* usec */
#define RPC_COSTS(ncpus)            2500            /* usec */

typedef struct replicate_hdr {
    int sender;
    int oid;
} replicate_hdr_t;

typedef struct migrate_hdr {
    int sender;
    int max_location;
    int oid;
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
    pan_msg_p msg;
};


static void r_replicate(int op, pan_upcall_p upcall, pan_msg_p request);
static void r_migrate(int op, pan_upcall_p upcall, pan_msg_p request);
static void r_fetch_obj(int op, pan_upcall_p upcall, pan_msg_p request);
static void r_delete_ref(int op, pan_upcall_p upcall, pan_msg_p request);

static int replicate_handle;
static int exclusive_handle;
static int migrate_handle;
static int fetch_handle;
static int delete_handle;

static float bcast_costs;
static float rpc_costs;


#define how( obj)    ((obj->fr_flags & MAN_LOCAL_ONLY) ? "local" : "bcast")

static pan_mutex_p migrate_synch;    /* Needed for handshake on migration */
static int rpc_ready, grp_ready;
static int man_alive = 0;
static pan_mutex_p strategy_lock;
static pan_cond_p strategy_cond;     /* Needed to make sure that all strategy
					calls are processed before OrcaMain
					forks (provided of course that the
					strategy calls are done before the
					forks).
				     */

/************/

static int
man_mcast_cont(void *state, pan_mutex_p lock)
{
    struct mcast_cont *mc;

    /* Koen : ad hoc code to avoid sending out group messages when the RTS
     * has already shut down the Panda layer because all Orca processes 
     * terminated. (Between creating a continuation and handling it by the 
     * cont_immediate_worker thread, other exit messages can be processed.)
     */
    if (man_alive) {
    	pan_mutex_unlock(lock);
    	mc = (struct mcast_cont *)state;
    	rc_mcast(mc->handle, mc->msg);
    	pan_mutex_lock(lock);
    }
else printf( "%d) gotcha\n", rts_my_pid);
    return CONT_NEXT;
}


static void
man_mcast(int handle, pan_msg_p msg)
{
    struct mcast_cont *mc;

    /*
     * Build a continuation and hand it off to the worker thread.
     */
    pan_mutex_lock(cont_immediate_lock);
    mc = (struct mcast_cont *)cont_alloc(&cont_immediate_queue, sizeof(*mc),
                                                man_mcast_cont);
    mc->handle = handle;
    mc->msg    = msg;
    cont_save(mc, 0);
    pan_mutex_unlock(cont_immediate_lock);
}

/************/
 
void
man_start(void)
{
    /* Make sure manager flag does not clash with ordinary fragment flags */
    assert( (MAN_FLAGS & RO_MASK) == 0);    

    man_alive = 1;
    rpc_ready = 0;
    grp_ready = 0;
    migrate_synch = pan_mutex_create();
    strategy_lock = pan_mutex_create();
    strategy_cond = pan_cond_create(strategy_lock);

    replicate_handle = rc_export(r_replicate);
    exclusive_handle = rc_export(r_migrate);
    migrate_handle   = rc_export(r_migrate);
    fetch_handle     = rc_export(r_fetch_obj);
    delete_handle    = rc_export(r_delete_ref);

    bcast_costs = BCAST_COSTS(rts_nr_platforms);
    rpc_costs   = RPC_COSTS(rts_nr_platforms);
}


void
man_end(void)
{
    man_alive = 0;
    pan_mutex_clear(migrate_synch);
    pan_cond_clear(strategy_cond);
    pan_mutex_clear(strategy_lock);
}


void
man_init(manager_p man, float reads, float writes)
{
    float access_sum = reads+writes;

    man->compiler.write_sum = writes;
    man->compiler.access_sum = access_sum;
    man->compiler.max_access = access_sum;
    man->compiler.max_location = rts_my_pid;
    man->compiler.cache_bcast = writes * bcast_costs;
    man->compiler.cache_rpc   = 0;
 
    man->runtime.write_sum = 0;
    man->runtime.access_sum = 0;
    man->runtime.max_access = 0;
    man->runtime.max_location = rts_my_pid;
}


void
man_clear(manager_p man)
{
}


/* Special unlock routine necessary to be able to free objects while holding a
 * lock.
 */


void
man_unlock(fragment_p obj)
/* If the system-wide reference count has dropped to zero, the object can
 * safely be removed from the object-table and deallocated.
 */
{
    int saved_id;

    if (obj->fr_total_refs == 0) {
#ifdef RTS_VERBOSE
	printf( "%d: delete object %s\n", rts_my_pid, obj->fr_name);
#endif
        f_unlock(obj);

        if (obj->fr_manager != NULL) {
            man_clear( obj->fr_manager);
            m_free(obj->fr_manager);
            obj->fr_manager = NULL;
        }
        if (obj->fr_fields != NULL) {
            if ( obj->fr_flags & MAN_VALID_FIELD)
	      if (td_objinfo(obj->fr_type)->obj_rec_free)
		(*(td_objinfo(obj->fr_type)->obj_rec_free))(obj->fr_fields);
            m_free(obj->fr_fields);
            obj->fr_fields = NULL;
        }

	saved_id = obj->fr_oid;    /* o_kill_rtsdep invalidates obj */
        o_kill_rtsdep(obj);        /* destroy RTS info */
	otab_remove(saved_id);     /* remove from obj. table (if present) */
    } else {
        f_unlock(obj);
    }
}


void
man_strategy( fragment_p obj, int replicated, int cpu)
{
    manager_p man;
    
    if (ignore_strategy)
	return;

    f_lock(obj);

    /* Hack for dynamic arrays; __Score() is called before
     * the user has had a chance to initialize the array.
     * Call __Score() again with the initialized object.
     */
    if (f_get_status( obj) == f_unshared)
        __Score( obj, obj->fr_type, (double) 0, (double) 0, (double) 481);
    
    man = obj->fr_manager;
    
    if (replicated) {
        /* fake a large number of reads by many procs */
        man->compiler.access_sum += BIG;
        man->compiler.cache_rpc
		= (man->compiler.access_sum - man->compiler.max_access)
						* rpc_costs;
        man->runtime.access_sum += BIG;
	cpu = rts_base_pid;
    } else {
        /* fake a large number of writes by 'cpu' */
        man->compiler.write_sum += BIG;
        man->compiler.access_sum += BIG;
        man->compiler.max_access = BIG;
        man->compiler.max_location = cpu;
        man->compiler.cache_bcast = man->compiler.write_sum * bcast_costs;
        man->compiler.cache_rpc   = (man->compiler.access_sum - BIG)
						* rpc_costs;
 
        man->runtime.write_sum += BIG;
        man->runtime.access_sum += BIG;
        man->runtime.max_access = BIG;
        man->runtime.max_location = cpu;
    }
    f_unlock(obj);
    /* We abuse DoFork to place the object as desired.
     */
    DoFork(cpu - rts_base_pid, &rts_proc_descr, (void **)&obj);
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

char *
man_unmarshall( char *buf, fragment_p obj)
{
    (void)memmove(obj->fr_manager, buf, sizeof(manager_t));
    return( buf + sizeof( manager_t));
}


/* process piggy back info; object has to be locked by caller */


void
man_get_piggy_info( fragment_p obj, man_piggy_p info)
{
    info->cpu = rts_my_pid;
    info->p_info = obj->fr_info;
    obj->fr_info.delta_reads = 0;
}


void
man_pack_piggy_info( pan_msg_p msg, man_piggy_p info)
{
    man_piggy_p piggy;

    piggy = (man_piggy_p)pan_msg_push(msg, sizeof(man_piggy_t),
				      alignof(man_piggy_t));
    *piggy = *info;
}


void
man_unpack_piggy_info( man_piggy_p piggy, fragment_p obj)
{
    if ( f_get_status(obj) & RO_MANAGER) {
        manager_p man = obj->fr_manager;
        
        if (piggy->p_info.access_sum > man->compiler.max_access) {
            man->compiler.max_access   = piggy->p_info.access_sum;
            man->compiler.max_location = piggy->cpu;
            man->compiler.cache_rpc
		= (man->compiler.access_sum - man->compiler.max_access)
						* rpc_costs;
        }

	/* The man_tick() macro does not update max_accesses, so
	 * man_take_decision works with out-of-date information. Occasionally
	 * this leads to unnecessary object replication/migration decisions.
	 * Trick: count coming operation if the piggy info is from the 
	 * max_location cpu.
	 */
	if (piggy->cpu == man->runtime.max_location) {
		/* Be as precise as possible */
		if (piggy->cpu == rts_my_pid) {
                	man->runtime.max_access = obj->fr_info.nr_accesses + 1;
		} else {
                	man->runtime.max_access = piggy->p_info.nr_accesses + 1;
		}
	}
	/* Avoid moving some (barrier) object around in an iterative
	 * application by updating the maximum only if the new value
	 * exceeds the current best by at least 10%.
	 */
        else if (piggy->p_info.nr_accesses > ((float)1.1)*man->runtime.max_access) {
                man->runtime.max_access = piggy->p_info.nr_accesses;
                man->runtime.max_location = piggy->cpu;
        }
        man->runtime.access_sum += piggy->p_info.delta_reads;
    }
}


/* process creation/termination of object references; locking by callee */
void
man_inc(fragment_p obj, int src, int dst, float reads, float writes)
{
    manager_p man;
    float access_sum = reads+writes;
    float cpu_access = access_sum;

    man_lock( obj);

    if (dst == rts_my_pid)
        cpu_access = (obj->fr_info.access_sum += access_sum);

    obj->fr_total_refs++;

    if (f_get_status(obj) & RO_MANAGER) {
        assert( obj->fr_manager);
        man = obj->fr_manager;
        man->compiler.write_sum += writes;
        man->compiler.access_sum += access_sum;
        if (cpu_access > man->compiler.max_access) {
            man->compiler.max_access = cpu_access;
            man->compiler.max_location = dst;
        }
        man->compiler.cache_bcast = man->compiler.write_sum * bcast_costs;
        man->compiler.cache_rpc
		= (man->compiler.access_sum - man->compiler.max_access)
					* rpc_costs;
        man_take_decision( obj);
    }

    /* Help, new Orca process fetches data of shared object object but
     * "forgets" to fetch continuations (guarded operations) as well.
     * Hack: empty continuation queue by temporarily setting status to
     * illegal. All guarded operations will be broadcast again.
     */
    if ((f_get_status(obj) & RO_REPLICATED) &&
	cont_pending(f_get_queue(obj))) {
	f_status_t stat = f_get_status(obj);

	obj->fr_flags &= ~RO_MASK;
	obj->fr_flags |= F_TEMP_ILL;
        cont_resume(f_get_queue(obj));
	assert( !cont_pending(f_get_queue(obj)));
	obj->fr_flags &= ~RO_MASK;
	obj->fr_flags |= stat;
    }

    man_unlock( obj);
}


void
man_dec( fragment_p obj, int cpu, float reads, float writes)
{
    manager_p man;
    float access_sum = reads+writes;

    man_lock( obj);
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
            /* by lack of better info, take manager self */
            man->compiler.max_access = obj->fr_info.access_sum;
            man->compiler.max_location = rts_my_pid;

	    man->runtime.max_access = obj->fr_info.nr_accesses;
	    man->runtime.max_location = rts_my_pid;
        }
        man->compiler.cache_bcast = man->compiler.write_sum * bcast_costs;
        man->compiler.cache_rpc
		= (man->compiler.access_sum - man->compiler.max_access)
						* rpc_costs;
        /* Don't; avoid unnecessary moves on program termination 
         *
         * if ( man->glob_ref>0)
         *    man_take_decision( obj);
         */
    }
    man_unlock( obj);
}


void
man_delete( fragment_p objects, float reads, float writes, int nr)
{
    int i;
    pan_msg_p msg = pan_msg_create();
    delete_hdr_t *hdr;
    int do_delete = 0;
    int *len;


    for ( i=0; i< nr; i++) {
	fragment_p f = &objects[i];

	/* Note that it is possible with arrays of (shared) objects that
	 * Score() is never called, so objects do not have to be removed
	 * from the object table.
	 */
	f_lock(f);
	if ( f_get_status(f) != f_unshared) {
            if ( (f->fr_flags & MAN_LOCAL_ONLY) == 0) {
	        do_delete++;

                hdr = pan_msg_push(msg, sizeof(delete_hdr_t),
					alignof(delete_hdr_t));
                hdr->writes = writes;
                hdr->reads  = reads;
                hdr->sender = rts_my_pid;
	        hdr->oid    = f->fr_oid;
	        f_unlock(f);
            } else {
	        f_unlock(f);
                man_dec(f, rts_my_pid, reads, writes);
            }
	    f->o_rtsdep = 0;		/* fool o_free() */
	    /* don't unlock f: f is dead. */
	} else {
	    f_unlock(f);
	}
    }
    if ( do_delete > 0) {
	/* no man_mcast() because this message has to be sent out before
	 * the exit message!
	 */
        len = pan_msg_push(msg, sizeof(int), alignof(int));
	*len = do_delete;
        rc_mcast(delete_handle, msg);
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
    float rt_bcast, rt_rpc;
    int rt_location;
 
    assert(!pan_mutex_trylock(f_get_lock(obj)));

    if ( obj->fr_flags & MAN_FUZZY_STATE)
        return;
 
    assert(obj->fr_owner == rts_my_pid);
 
    /* Local info is never piggybacked, so process it now to
     * avoid taking a wrong decision. Use ">=", not ">", to
     * keep the manager at home when the access counts are equal.
     */
    if ( obj->fr_info.nr_accesses >= man->runtime.max_access) {
        man->runtime.max_access = obj->fr_info.nr_accesses;
        man->runtime.max_location = rts_my_pid;
    }

    /* Compute costs of both object states (replicated or not) for
     * two heuristics (compiler vs. rts).
     */
    bcast = man->compiler.cache_bcast;
    rpc   = man->compiler.cache_rpc;
    location = man->compiler.max_location;

    /* If compiler info and rts accounting should be combined, a conflict can
     * occur. Either both methods do not agree on whether or not to replicate
     * an object, or they disagree about the location of an object in single
     * copy mode. We take the decision of the heuristic that shows the largest
     * difference between the costs for replicating and single copy mode.
     */
    if ( use_runtime_info) {
        /* Grr, problems with overflow, so immediately convert to float */
        rt_bcast = man->runtime.write_sum * bcast_costs;
        rt_rpc   = (man->runtime.access_sum - man->runtime.max_access)
                   					* rpc_costs;
	rt_location = man->runtime.max_location;
 
        if ( !use_compiler_info || 
	     /* Only use runtime info if enough statistics are available.
	      * Wait at least until "everybody" has accessed the object once.
	      * Two different cases: replicated and single copy mode
	      */
	     (man->runtime.access_sum >= man->runtime.max_access
				+ (bcast<rpc ? obj->fr_total_refs-1 : 1)
	     &&
    	     /* Avoid thrashing between compiler and rts decisions, so
     	      * use a threshold (1.1).
     	      */
    	     (bcast>rpc ? (rt_bcast<rt_rpc ?
			      /* confict about replication policy
			       */
			           (rt_rpc*rpc>((float)1.1)*bcast*rt_bcast)
	    		      /* agreement about single copy mode, but where?
			       */
			         : (rt_bcast*rpc>((float)1.1)*bcast*rt_rpc))
		        :
     	      bcast==rpc   /* no compiler preference, use runtime statistics */
		        ||
     	    /*bcast<rpc*/ (rt_bcast>rt_rpc &&
			      /* confict about replication policy
			       */
	   			   (rt_bcast*bcast>((float)1.1)*rpc*rt_rpc))))) {
            bcast = rt_bcast;
            rpc = rt_rpc;
            location = rt_location;
        }
    }

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
	    sprintf( buf, "%s: %s exclusive at %d (W=%g, A=%g, M=%g)",
			  how(obj), obj->fr_name, location,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
	    trc_event( man_decision, buf);
#endif
	    if (verbose) 
	        printf( "%s decision: %s exclusive at %d (W=%g, A=%g, M=%g)\n",
			  how(obj), obj->fr_name, location,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
            /* only broadcast decision for external refs */
            if ( (obj->fr_flags & MAN_LOCAL_ONLY) == 0) {
                pan_msg_p msg = pan_msg_create();
                migrate_hdr_t *hdr;
        
                hdr = pan_msg_push(msg, sizeof(migrate_hdr_t),
				   alignof(migrate_hdr_t));
                hdr->sender = rts_my_pid;
                hdr->max_location = location;
		hdr->oid = obj->fr_oid;
                man_mcast(exclusive_handle, msg);
        
                obj->fr_flags |= MAN_FUZZY_STATE;
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
	    sprintf( buf, "%s: %s replicate (W=%g, A=%g, M=%g)",
			  how(obj), obj->fr_name,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
	    trc_event( man_decision, buf);
#endif
	    if (verbose) 
	        printf( "%s decision: %s replicate (W=%g, A=%g, M=%g)\n",
			  how(obj), obj->fr_name,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
            obj->fr_flags |= RO_REPLICATED;
            /* only broadcast decision for external refs */
            if ( (obj->fr_flags & MAN_LOCAL_ONLY) == 0) {
                pan_msg_p msg = pan_msg_create();
                replicate_hdr_t *hdr;
        
                mm_pack_sh_object( msg, obj);
                hdr = pan_msg_push(msg, sizeof(replicate_hdr_t),
				   alignof(replicate_hdr_t));
                hdr->sender = rts_my_pid;
                hdr->oid    = obj->fr_oid;
                man_mcast(replicate_handle, msg);

		/* Problem: hand replicate message to low priority thread,
		 * Orca process then broadcast a write operation, which gets
		 * out of the door before the replicate message => update is
		 * lost on all machines (except locally)
		 */
                obj->fr_flags &= ~RO_MASK;	 /* invalid until BC is back */
                obj->fr_flags |= F_IN_TRANSIT;
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
	    sprintf( buf, "%s: %s migrate to %d (W=%g, A=%g, M=%g)",
			  how(obj), obj->fr_name, location,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
	    trc_event( man_decision, buf);
#endif
	    if (verbose) 
	        printf( "%s decision: %s migrate to %d (W=%g, A=%g, M=%g)\n",
			  how(obj), obj->fr_name, location,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
            /* only broadcast decision for external refs */
            if ( (obj->fr_flags & MAN_LOCAL_ONLY) == 0) {
                pan_msg_p msg = pan_msg_create();
                migrate_hdr_t *hdr;
        
                hdr = pan_msg_push(msg, sizeof(migrate_hdr_t),
				   alignof(migrate_hdr_t));
                hdr->sender = rts_my_pid;
                hdr->max_location = location;
		hdr->oid = obj->fr_oid;

                man_mcast(migrate_handle, msg);
        
                obj->fr_flags |= MAN_FUZZY_STATE;
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


static int
cont_await_migration(void *state, pan_mutex_p lock)
{
    return CONT_NEXT;
}


void
man_await_migration(fragment_p f)
{
    void *buf;

    /* check outside in DoOperation loop */
    /* block until the transient state has changed */
    buf = cont_alloc(f_get_queue(f), 0, cont_await_migration);
    cont_save(buf, 1);
}


/***** service routines *****/


static void
r_replicate(int handle, pan_upcall_p upcall, pan_msg_p request)
{
    replicate_hdr_t *hdr;
    fragment_p obj;

    assert(handle == replicate_handle);
    hdr = pan_msg_pop(request, sizeof(replicate_hdr_t),
		      alignof(replicate_hdr_t));

    if ((obj = otab_lookup(hdr->oid))) {
#ifdef TRACING
        char buf[256];
 
        sprintf( buf, "%80s", " ");
        sprintf( buf, "change status: %s replicated", obj->fr_name);
        trc_event( man_change, buf);
#endif

        man_lock( obj);
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
	    obj->fr_flags &= ~RO_MASK;
	    obj->fr_flags |= F_MANAGER;
	    cont_resume(f_get_queue(obj));
	} else {
            mm_unpack_sh_object( request, obj);
	    assert( f_get_status(obj) == f_replicated);
	}
        man_unlock( obj);
    }
    rc_mcast_done();
}




static int
r_migr_cont(void *state, pan_mutex_p lock)
{
    struct migr_cont *mc = (struct migr_cont *)state;
    fragment_p obj = mc->obj;
    int src = mc->src;
    pan_msg_p req, rep;

    /* let go of lock to avoid deadlock when man_take_decision() wants
     * to deposit a job in the queue.
     */
    pan_mutex_unlock(lock);

    req = pan_msg_create();
    tm_push_int(req, obj->fr_oid);
    tm_push_int(req, rts_my_pid);

#ifdef RTS_VERBOSE
    printf("%d) r_migr_cont: fetching remote object\n", rts_my_pid);
#endif
    /* fetch object data */
    rc_rpc(src, fetch_handle, req, &rep); 

    /* IMPORTANT: lock is grabbed after RPC to avoid
     * blocking the communication daemon.
     */
    man_lock( obj);
    mm_unpack_sh_object( rep, obj);
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

    /* manager transfer is completed at all sites */
    obj->fr_flags &= ~MAN_FUZZY_STATE;
    obj->fr_flags &= ~RO_REPLICATED;
    obj->fr_owner = rts_my_pid;
    cont_resume(f_get_queue(obj));

    /* Something might have changed between taking the decision to
     * migrate the object and the actual data transfer.
     */
    man_take_decision( obj);
 
    man_unlock( obj);

    pan_msg_clear(req);
    pan_msg_clear(rep);

    rc_mcast_done();
    pan_mutex_lock(lock);
    return CONT_NEXT;
}


static void
r_migrate(int handle, pan_upcall_p dummy, pan_msg_p request)
{
    fragment_p obj;
    int src, cpu, mc_done = 1;
    migrate_hdr_t *hdr;
    struct migr_cont *mc;

    assert(handle == migrate_handle || handle == exclusive_handle);
    
    hdr = pan_msg_pop(request, sizeof(migrate_hdr_t), alignof(migrate_hdr_t));

    if ((obj = otab_lookup(hdr->oid))) { /* do we know this object at all? */
    
        src = hdr->sender;
        cpu = hdr->max_location;
        if (cpu == rts_my_pid && 
            src != rts_my_pid) { /* object is put on this node */
            /*
             * Build a continuation and hand it off to the worker thread.
             */
            pan_mutex_lock(cont_immediate_lock);
            mc = (struct migr_cont *)cont_alloc(&cont_immediate_queue,
                                                sizeof(*mc),
                                                r_migr_cont);
            mc->src = src;
            mc->obj = obj;
            cont_save(mc, 0);
            pan_mutex_unlock(cont_immediate_lock);
            mc_done = 0;
#ifdef RTS_VERBOSE
	    printf("%d) r_migrate: built continuation\n", rts_my_pid);
#endif
        } else {
            man_lock(obj);

#ifdef TRACING
            {
                char buf[256];
 
                sprintf( buf, "%80s", " ");
                sprintf( buf, "change status: %s exclusive at %d",
                              obj->fr_name, cpu);
                trc_event( man_change, buf);
            }
#endif
    
            /* manager transfer is completed at all sites */
            obj->fr_flags &= ~MAN_FUZZY_STATE;
            obj->fr_flags &= ~RO_REPLICATED;
            obj->fr_owner = cpu;
            if (cpu != rts_my_pid) {
                obj->fr_flags &= ~RO_MANAGER;
            } else {
                /* Something might have changed between taking the decision to
                 * migrate the object and the actual data transfer.
                 */
                assert( obj->fr_flags & RO_MANAGER);
                man_take_decision( obj);
            }
            cont_resume(f_get_queue(obj));
        
            man_unlock( obj);
        
            /* Am i the source, but not destination, of the object? */
            if (src == rts_my_pid && cpu != rts_my_pid) {
                pan_mutex_lock(migrate_synch);
                if (!rpc_ready) { /* group listener first, wait for RPC */
                    grp_ready++;
		    mc_done = 0;
                } else {
                    rpc_ready = 0;
		}
                pan_mutex_unlock(migrate_synch);
	    }
        }
    }
    if (mc_done) {
	rc_mcast_done();
    }	
}


static void
r_fetch_obj(int handle, pan_upcall_p upcall, pan_msg_p request)
{
    int oid;
    fragment_p obj;
    pan_msg_p reply;
    int cpu, mc_done;

    assert(handle == fetch_handle);
    tm_pop_int(request, &cpu);
    tm_pop_int(request, &oid);
    obj = otab_lookup(oid);
    assert(obj);
#ifdef RTS_VERBOSE
    printf("%d: r_fetch_obj: returning obj %s, fields = %p\n",
	   rts_my_pid, obj->fr_name, obj->fr_fields);
#endif
    reply = pan_msg_create();
    man_lock(obj);
    /* problem: RPC might arrive before issuing BC is handled locally,
     *          so owner field is still set to this node and the manager
     *          state won't be marshalled.
     */
    obj->fr_owner = cpu;
    mm_pack_sh_object(reply, obj);
    assert(obj->fr_manager == NULL);
    man_unlock(obj);

    rc_untagged_reply(upcall, reply);
    pan_msg_clear(request);
 
    /* Synchronize with r_migrate().
     */
    pan_mutex_lock(migrate_synch);
    if (grp_ready) {
        /* r_migrate went first, _now_ it is done */
        grp_ready = 0;
        mc_done = 1;
    } else {
        /* I got here first, r_migrate can proceed */
        rpc_ready++;
        mc_done = 0;
    }
    pan_mutex_unlock(migrate_synch);
    if (mc_done) {
        rc_mcast_done();
    }
}


static void
r_delete_ref(int handle, pan_upcall_p dummy, pan_msg_p request)
/*
 * needed to adjust reference count for "local" objects; parameters are
 * dealt with by r_exit().
 */
{
    fragment_p obj;
    delete_hdr_t *hdr;
    int *len;
    int do_delete;
    int i;

    assert(handle == delete_handle);

    len = pan_msg_pop(request, sizeof(int), alignof(int));
    do_delete = *len;
    for ( i=0; i<do_delete; i++) {
        hdr = pan_msg_pop(request, sizeof(delete_hdr_t), alignof(delete_hdr_t));
    
        if ((obj = otab_lookup(hdr->oid))) {
            man_dec(obj, hdr->sender, hdr->reads, hdr->writes);
        }
    }
    rc_mcast_done();
}
