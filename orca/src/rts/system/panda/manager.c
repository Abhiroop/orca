#include <string.h>
#include <stdio.h>
#include "panda/panda.h"
#include "continuation.h"
#include "fragment.h"
#include "manager.h"
#include "msg_marshall.h"
#include "rts_comm.h"
#include "obj_tab.h"
#include "rts_util.h"
#include "rts_internals.h"
#include "interface.h"
#include "limits.h"
#include "rts_trace.h"

#define BCAST_COSTS(ncpus)    (2700+ncpus*7)      /* usec */
#define RPC_COSTS(ncpus)            2500            /* usec */

#define BIG  ((double) 1.0e+55)      /* constant to force object placement */
#define BIG_INT (1<<27)              /* constant to force object placement */

typedef struct replicate_hdr {
    int   sender;
    oid_t oid;
} replicate_hdr_t;

typedef struct migrate_hdr {
    int   sender;
    int max_location;
    oid_t oid;
} migrate_hdr_t;

typedef struct delete_hdr {
    double writes;
    double reads;
    int    sender;
    oid_t  oid;
} delete_hdr_t;

typedef struct move_hdr {
    int dst;
    int src;
    int type_reg;
    oid_t oid;
} move_hdr_t;

struct migr_cont {
    int src;
    fragment_p obj;
};

struct mcast_cont {
    int server;
    int op;
    message_p msg;
};


static void r_replicate(int op, pan_upcall_t upcall, message_p request);
static void r_migrate(int op, pan_upcall_t upcall, message_p request);
static void r_fetch_obj(int op, pan_upcall_t upcall, message_p request);
static void r_delete_ref(int op, pan_upcall_t upcall, message_p request);
static void r_move(int op, pan_upcall_t upcall, message_p request);

static operation_p man_operations[] = { r_replicate,
                    r_migrate,
                    r_migrate,
                    r_fetch_obj,
                    r_delete_ref,
                    r_move};
#define REPLICATE    0
#define EXCLUSIVE    1
#define MIGRATE      2
#define FETCH_OBJ    3
#define DELETE_REF   4
#define MOVE         5


#define how( obj)    ((obj->fr_flags & MAN_LOCAL_ONLY) ? "local" : "bcast")

extern mpool_t message_pool;

static int man_server;

static mutex_t migrate_synch;        /* Needed for handshake on migration */
static int rpc_ready, grp_ready;
static cond_t migrate_done;	     /* To signal end of replication process */
static mutex_t strategy_lock;
static cond_t strategy_cond;	     /* Needed to make sure that all strategy
					calls are processed before OrcaMain
					forks (provided of course that the
					strategy calls are done before the
					forks).
				      */

/************/

static int
man_mcast_cont(void *state, mutex_t *lock)
{
    struct mcast_cont *mc;

    sys_mutex_unlock( lock);
    mc = (struct mcast_cont *)state;
    rc_mcast( mc->server, mc->op, mc->msg);
    sys_mutex_lock( lock);
    return CONT_NEXT;
}


static void
man_mcast( int server, int op, message_p msg)
{
    struct mcast_cont *mc;

    /*
     * Build a continuation and hand it off to the worker thread.
     */
    sys_mutex_lock( &cont_immediate_lock);
    mc = (struct mcast_cont *)cont_alloc(&cont_immediate_queue, sizeof(*mc),
                                                man_mcast_cont);
    mc->server = server;
    mc->op = op;
    mc->msg = msg;
    cont_save(mc, 0);
    sys_mutex_unlock( &cont_immediate_lock);
}

/************/
 
void
man_start( void)
{
    /* Make sure manager flag does not clash with ordinary fragment flags */
    assert( (MAN_FLAGS & RO_MASK) == 0);    

    rpc_ready = 0;
    grp_ready = 0;
    sys_mutex_init( &migrate_synch);
    sys_cond_init( &migrate_done);
    sys_mutex_init( &strategy_lock);
    sys_cond_init( &strategy_cond);

    man_server = rc_export( "man_server", 6, man_operations);
}


void
man_end( void)
{
    sys_mutex_clear( &migrate_synch);
    sys_cond_clear( &migrate_done);
    sys_mutex_clear( &strategy_lock);
    sys_cond_clear( &strategy_cond);
}


void
man_init( manager_p man, double reads, double writes)
{
    double access_sum = reads+writes;

    man->compiler.write_sum = writes;
    man->compiler.access_sum = access_sum;
    man->compiler.max_access = access_sum;
    man->compiler.max_location = sys_my_pid;
 
    man->runtime.write_sum = 0;
    man->runtime.access_sum = 0;
    man->runtime.max_access = 0;
    man->runtime.max_location = sys_my_pid;
}


void
man_clear( manager_p man)
{
}


/* Special unlock routine necessary to be able to free objects while holding a
 * lock.
 */


void
man_unlock( fragment_p obj)
/* If the system-wide reference count has dropped to zero, the object can
 * safely be removed from the object-table and deallocated.
 */
{
    if ( obj->fr_total_refs == 0) {
        otab_entry_p entry = otab_lookup( &(obj->fr_oid));

/* printf( "%d: delete object %s\n", sys_my_pid, obj->fr_name);
 */
        f_unlock( obj);

        if ( obj->fr_manager != NULL) {
            man_clear( obj->fr_manager);
            sys_free( obj->fr_manager);
            obj->fr_manager = NULL;
        }
        if ( obj->fr_fields != NULL) {
            if ( obj->fr_flags & MAN_VALID_FIELD)
                r_free(obj->fr_fields, td_objrec(obj->fr_type));
            sys_free( obj->fr_fields);
            obj->fr_fields = NULL;
        }

        o_kill_rtsdep(obj);         /* destroy RTS info */
 
        if (entry) {                /* if ever entered into obj.table... */
            otab_remove(entry);
        }
    } else {
        f_unlock( obj);
    }
}


void
man_strategy( fragment_p obj, int replicated, int cpu)
{
    manager_p man;
    
    f_lock(obj);

    /* Hack for dynamic arrays; __Score() is called before
     * the user has had a chance to initialize the array.
     * Call __Score() again with the initialized object.
     */
    if (f_get_status( obj) == f_unshared)
        __Score( obj, obj->fr_type, (double) 0, (double) 0, (double) 481);
    
    man = obj->fr_manager;
    
    assert( man && obj->fr_total_refs == 1);
    
    if ( replicated) {
        /* fake a large number of reads by many procs */
        man->compiler.access_sum += BIG;
        man->compiler.max_location = cpu;
 
        man->runtime.access_sum += BIG;
        man->runtime.max_location = cpu;

    
        man_take_decision( obj);
    
        if ( cpu != sys_my_pid) {
            obj->fr_owner = cpu;
            obj->fr_flags &= ~RO_MANAGER;
        }
    
        assert( obj->fr_flags & RO_REPLICATED);
    }
    else {
        assert( 1 <= cpu && cpu <= sys_nr_platforms);
    
        /* fake a large number of writes by 'cpu' */
        man->compiler.write_sum += BIG;
        man->compiler.access_sum += BIG;
        man->compiler.max_access = BIG;
        man->compiler.max_location = cpu;
 
        man->runtime.write_sum += BIG;
        man->runtime.access_sum += BIG;
        man->runtime.max_access = BIG;
        man->runtime.max_location = cpu;

        man_take_decision( obj);
    
        assert( !(obj->fr_flags & RO_REPLICATED));
    }
    
    if ( cpu != sys_my_pid) { /* ship manager */
        message_p msg = grp_message_init();
        move_hdr_t *hdr;
    
        mm_pack_sh_object(msg, obj);
        hdr = sys_message_push(msg, sizeof(move_hdr_t), alignof(move_hdr_t));
        hdr->dst = cpu;
	hdr->src = sys_my_pid;
        hdr->type_reg = td_registration(obj->fr_type);
        oid_copy(&obj->fr_oid, &hdr->oid);
	/* don't call man_mcast() because BC will not be sent out for
	 * a while, and manager assumes that this message will be delivered
	 * before any fork message.
	 */

    	f_unlock(obj);
	sys_mutex_lock(&strategy_lock);
        rc_mcast(man_server, MOVE, msg);
	sys_cond_wait(&strategy_cond, &strategy_lock);
	sys_mutex_unlock(&strategy_lock);
    } else {
    	f_unlock(obj);
    }
}


/***** marshalling code *****/


int
man_nbytes( fragment_p obj)
{
    return( sizeof( manager_t));
}


char *
man_marshall( char *buf, fragment_p obj)
{
    (void)memmove(buf, obj->fr_manager, sizeof(manager_t));
    man_clear( obj->fr_manager);
    sys_free( obj->fr_manager);
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
    info->cpu = sys_my_pid;
    info->p_info = obj->fr_info;
    obj->fr_info.delta_reads = 0;
}


void
man_pack_piggy_info( message_p msg, man_piggy_p info)
{
    man_piggy_p piggy;

    piggy = (man_piggy_p)sys_message_push(msg, sizeof(man_piggy_t),
                                          alignof(man_piggy_t));
    *piggy = *info;
}


void
man_unpack_piggy_info( message_p msg, fragment_p obj)
{
    if ( f_get_status(obj) & RO_MANAGER) {
        manager_p man = obj->fr_manager;
        man_piggy_p piggy;

	/* Process piggybacked info from outside */
        piggy = (man_piggy_p)sys_message_pop(msg, sizeof(man_piggy_t),
                                             alignof(man_piggy_t));
        
        if (piggy->p_info.access_sum > man->compiler.max_access) {
            man->compiler.max_access   = piggy->p_info.access_sum;
            man->compiler.max_location = piggy->cpu;
        }

	/* Avoid moving some (barrier) object around in an iterative
	 * application by updating the maximum only if the new value
	 * exceeds the current best by at least 2.
	 */
        if ( piggy->p_info.nr_accesses > man->runtime.max_access+1) {
                man->runtime.max_access = piggy->p_info.nr_accesses;
                man->runtime.max_location = piggy->cpu;
        }
        man->runtime.access_sum += piggy->p_info.delta_reads;
    }
}


/* process creation/termination of object references; locking by callee */
void
man_inc( fragment_p obj, int src, int dst, double reads, double writes)
{
    manager_p man;
    double access_sum = reads+writes;
    double cpu_access = access_sum;

    man_lock( obj);

    if ( dst == sys_my_pid)
        cpu_access = (obj->fr_info.access_sum += access_sum);

    if ( src != sys_my_pid || dst == sys_my_pid)
        obj->fr_total_refs++;
    /* ELSE update the reference count when the object is marshalled to
     * avoid race between RPC and BC, see r_fetch().
     */

    if ( f_get_status(obj) & RO_MANAGER) {
        assert( obj->fr_manager);
        man = obj->fr_manager;
        man->compiler.write_sum += writes;
        man->compiler.access_sum += access_sum;
        if ( cpu_access>man->compiler.max_access) {
            man->compiler.max_access = cpu_access;
            man->compiler.max_location = dst;
        }
        man_take_decision( obj);
    }

    /* Help, new Orca process fetches data of shared object object but
     * "forgets" to fetch continuations (guarded operations) as well.
     * Hack: empty continuation queue by temporarily setting status to
     * illegal. All guarded operations will be broadcast again.
     */
    if ((f_get_status(obj) & RO_REPLICATED) && cont_pending(f_get_queue(obj))) {
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
man_dec( fragment_p obj, int cpu, double reads, double writes)
{
    manager_p man;
    double access_sum = reads+writes;

    man_lock( obj);
    obj->fr_total_refs--;
    if ( cpu == sys_my_pid) {
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
            man->compiler.max_location = sys_my_pid;

	    man->runtime.max_access = obj->fr_info.nr_accesses;
	    man->runtime.max_location = sys_my_pid;
        }
        /* Don't; avoid unnecessary moves on program termination 
         *
         * if ( man->glob_ref>0)
         *    man_take_decision( obj);
         */
    }
    man_unlock( obj);
}


void
man_delete( fragment_p obj, double reads, double writes)
{
    /* Avoid a flood of delete messages in case of objects that
     * are allocated but actually never used outside this platform.
     *
     * Bottom line: don't send too many group messages asynchronously!
     */
    if ( (obj->fr_flags & MAN_LOCAL_ONLY) == 0) {
        message_p msg = grp_message_init();
        delete_hdr_t *hdr;
    
        hdr = sys_message_push(msg, sizeof(delete_hdr_t),
                               alignof(delete_hdr_t));
        hdr->writes = writes;
        hdr->reads  = reads;
        hdr->sender = sys_my_pid;
        oid_copy(&obj->fr_oid, &hdr->oid);
	/* no man_mcast() because this message has to be sent out before
	 * the exit message!
	 */
	f_unlock(obj);
        rc_mcast(man_server, DELETE_REF, msg);
    } else {
	f_unlock(obj);
        man_dec( obj, sys_my_pid, reads, writes);
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
    extern int replicate_none, replicate_all;
    extern int use_compiler_info, use_runtime_info;
    manager_p man = obj->fr_manager;
    double bcast, rpc;
    int location;
    double rt_bcast, rt_rpc;
    int rt_location;
 
    assert(!sys_mutex_trylock(f_get_lock(obj)));

    if ( obj->fr_flags & MAN_FUZZY_STATE)
        return;
 
    assert( obj->fr_owner == sys_my_pid);
 
    /* Local info is never piggybacked, so process it now to
     * avoid taking a wrong decision.
     */
    if (obj->fr_info.access_sum > man->compiler.max_access) {
        man->compiler.max_access   = obj->fr_info.access_sum;
        man->compiler.max_location = sys_my_pid;
    }
    else if (obj->fr_info.access_sum == man->compiler.max_access) {
	/* Keep object at home if nr_accesses equal */
        man->compiler.max_location = sys_my_pid;
    }

    if ( obj->fr_info.nr_accesses > man->runtime.max_access) {
        man->runtime.max_access = obj->fr_info.nr_accesses;
        man->runtime.max_location = sys_my_pid;
    }
    else if (obj->fr_info.nr_accesses == man->runtime.max_access) {
	/* Keep object at home if nr_accesses equal */
        man->runtime.max_location = sys_my_pid;
    }

    /* Compute costs of both object states (replicated or not) for
     * two heuristics (compiler vs. rts).
     */
    bcast = man->compiler.write_sum * BCAST_COSTS(sys_nr_platforms);
    rpc   = (man->compiler.access_sum - man->compiler.max_access)
                        * RPC_COSTS(sys_nr_platforms);
    location = man->compiler.max_location;
    rt_bcast = man->runtime.write_sum * BCAST_COSTS(sys_nr_platforms);
    rt_rpc   = (man->runtime.access_sum - man->runtime.max_access)
                        * RPC_COSTS(sys_nr_platforms);
    rt_location = man->runtime.max_location;
 
    /* If compiler info and rts accounting should be combined, a conflict can
     * occur. We take the decision of the heuristic that shows the largest
     * difference between the costs for replicating and single copy mode.
     */
    if ( use_runtime_info)
        if ( !(use_compiler_info &&
               ((bcast>rpc && rt_bcast<rt_rpc && bcast*rt_bcast<rt_rpc*rpc) ||
		(bcast<rpc && rt_bcast>rt_rpc && rpc*rt_rpc<rt_bcast*bcast)))) {
            bcast = rt_bcast;
            rpc = rt_rpc;
            location = rt_location;
        }

    if ( f_get_status(obj) & RO_REPLICATED) {
        if ( rpc < bcast && !replicate_all) {
#ifdef TRACING
	    char buf[256];

	    sprintf( buf, "%80s", " ");
	    sprintf( buf, "%s decision: %s exclusive at %d (W=%g, A=%g, M=%g)",
			  how(obj), obj->fr_name, location,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
	    trc_event( man_decision, buf);
#elif VERBOSE
	    printf( "%s decision: %s exclusive at %d (W=%g, A=%g, M=%g)\n",
			  how(obj), obj->fr_name, location,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
#endif
            /* only broadcast decision for external refs */
            if ( (obj->fr_flags & MAN_LOCAL_ONLY) == 0) {
                message_p msg = grp_message_init();
                migrate_hdr_t *hdr;
        
                hdr = sys_message_push(msg, sizeof(migrate_hdr_t),
                                       alignof(migrate_hdr_t));
                hdr->sender = sys_my_pid;
                hdr->max_location = location;
                oid_copy(&obj->fr_oid, &hdr->oid);
                man_mcast(man_server, EXCLUSIVE, msg);
        
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
                if ( obj->fr_owner != sys_my_pid) {
                    obj->fr_flags &= ~RO_MANAGER;
                    /* object will be fetched by forkee */
                }
                cont_resume(f_get_queue(obj));
            }
        }
    } else {
        if (!replicate_none && (bcast < rpc || replicate_all)) {
#ifdef TRACING
	    char buf[256];

	    sprintf( buf, "%80s", " ");
	    sprintf( buf, "%s decision: %s replicate (W=%g, A=%g, M=%g)",
			  how(obj), obj->fr_name,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
	    trc_event( man_decision, buf);
#elif VERBOSE
	    printf( "%s decision: %s replicate (W=%g, A=%g, M=%g)\n",
			  how(obj), obj->fr_name,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
#endif
            obj->fr_flags |= RO_REPLICATED;
            cont_resume(f_get_queue(obj));
            /* only broadcast decision for external refs */
            if ( (obj->fr_flags & MAN_LOCAL_ONLY) == 0) {
                message_p msg = grp_message_init();
                replicate_hdr_t *hdr;
        
                mm_pack_sh_object( msg, obj);
                hdr = sys_message_push(msg, sizeof(replicate_hdr_t),
                                       alignof(replicate_hdr_t));
                hdr->sender = sys_my_pid;
                oid_copy(&(obj->fr_oid), &hdr->oid);
                man_mcast(man_server, REPLICATE, msg);

		/* Problem: hand replicate message to low priority thread,
		 * Orca process then broadcast a write operation, which gets
		 * out of the door before the replicate message => update is
		 * lost on all amchines (except locally)
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
        } else if (location != obj->fr_owner) {
#ifdef TRACING
	    char buf[256];

	    sprintf( buf, "%80s", " ");
	    sprintf( buf, "%s decision: %s migrate to %d (W=%g, A=%g, M=%g)",
			  how(obj), obj->fr_name, location,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
	    trc_event( man_decision, buf);
#elif VERBOSE
	    printf( "%s decision: %s migrate to %d (W=%g, A=%g, M=%g)\n",
			  how(obj), obj->fr_name, location,
			  man->runtime.write_sum, man->runtime.access_sum,
			  man->runtime.max_access);
#endif
            /* only broadcast decision for external refs */
            if ( (obj->fr_flags & MAN_LOCAL_ONLY) == 0) {
                message_p msg = grp_message_init();
                migrate_hdr_t *hdr;
        
                hdr = sys_message_push(msg, sizeof(migrate_hdr_t),
                                       alignof(migrate_hdr_t));
                hdr->sender = sys_my_pid;
                hdr->max_location = location;
                oid_copy(&obj->fr_oid, &hdr->oid);
                man_mcast(man_server, MIGRATE, msg);
        
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

void
man_await_migration(fragment_p f)
{
    /* check outside in DoOperation loop */
    sys_cond_wait( &migrate_done, f_get_lock(f));
}


/***** service routines *****/


static void
r_replicate(int op, pan_upcall_t upcall, message_p request)
{
    replicate_hdr_t *hdr;
    otab_entry_p entry;

    assert( op == REPLICATE);
    hdr = sys_message_pop(request, sizeof(replicate_hdr_t),
                          alignof(replicate_hdr_t));

    if ( (entry = otab_lookup(&hdr->oid))) {
        fragment_p obj = &(entry->frag);

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
	if ( hdr->sender == sys_my_pid) {
	    obj->fr_flags &= ~RO_MASK;
	    obj->fr_flags |= F_MANAGER;
	    sys_cond_broadcast( &migrate_done);
	} else {
            mm_unpack_sh_object( request, obj);
	    assert( f_get_status(obj) == f_replicated);
	}
        man_unlock( obj);
    }
    rc_mcast_done();
}




static int r_migr_cont(void *state, mutex_t *lock)
{
    struct migr_cont *mc = (struct migr_cont *)state;
    fragment_p obj = mc->obj;
    int src = mc->src;
    message_p req, rep;

    /* let go of lock to avoid deadlock when man_take_decision() wants
     * to deposit a job in the queue.
     */
    sys_mutex_unlock(lock);

    req = get_mpool(&message_pool);
    ru_push_oid(req, &obj->fr_oid);
    tm_push_int(req, sys_my_pid);

    /* fetch object data */
    rc_rpc(src, man_server, FETCH_OBJ, req, &rep); 

    /* IMPORTANT: lock is grabbed after RPC to avoid
     * blocking the communication daemon.
     */
    man_lock( obj);
    mm_unpack_sh_object( rep, obj);
    assert( obj->fr_flags & RO_MANAGER); /* I should be manager now */

#ifdef TRACING
    {
	char buf[256];

	sprintf( buf, "%80s", " ");
	sprintf( buf, "change status: %s exclusive at %d",
		      obj->fr_name, sys_my_pid);
	trc_event( man_change, buf);
    }
#endif

    /* manager transfer is completed at all sites */
    obj->fr_flags &= ~MAN_FUZZY_STATE;
    obj->fr_flags &= ~RO_REPLICATED;
    obj->fr_owner = sys_my_pid;
    cont_resume(f_get_queue(obj));

    /* Something might have changed between taking the decision to
     * migrate the object and the actual data transfer.
     */
    man_take_decision( obj);
 
    man_unlock( obj);

    sys_message_clear(req);
    sys_message_clear(rep);

    rc_mcast_done();
    sys_mutex_lock(lock);
    return CONT_NEXT;
}


static void
r_migrate(int op, pan_upcall_t dummy, message_p request)
{
    otab_entry_p entry;
    int src, cpu, mc_done = 1;
    migrate_hdr_t *hdr;
    struct migr_cont *mc;

    assert( op == MIGRATE || op == EXCLUSIVE);
    
    hdr = sys_message_pop(request, sizeof(migrate_hdr_t),
              alignof(migrate_hdr_t));

    if ((entry = otab_lookup(&hdr->oid))) { /* do we know this object at all? */
        fragment_p obj = &(entry->frag);
    
        src = hdr->sender;
        cpu = hdr->max_location;
        if (cpu == sys_my_pid && 
            src != sys_my_pid) { /* object is put on this node */
            /*
             * Build a continuation and hand it off to the worker thread.
             */
            sys_mutex_lock( &cont_immediate_lock);
            mc = (struct migr_cont *)cont_alloc(&cont_immediate_queue,
                                                sizeof(*mc),
                                                r_migr_cont);
            mc->src = src;
            mc->obj = obj;
            cont_save(mc, 0);
            sys_mutex_unlock( &cont_immediate_lock);
            mc_done = 0;
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
            if ( cpu != sys_my_pid) {
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
            if (src == sys_my_pid && cpu != sys_my_pid) {
                sys_mutex_lock( &migrate_synch);
                if (!rpc_ready) { /* group listener first, wait for RPC */
                    grp_ready++;
		    mc_done = 0;
                } else {
                    rpc_ready = 0;
		}
                sys_mutex_unlock( &migrate_synch);
	    }
        }
    }
    if (mc_done) {
	rc_mcast_done();
    }	
}


static void
r_fetch_obj(int op, pan_upcall_t upcall, message_p request)
{
    oid_t id;
    otab_entry_p entry;
    fragment_p obj;
    message_p reply;
    int cpu, mc_done;

    assert( op == FETCH_OBJ);
    tm_pop_int( request, &cpu);
    ru_pop_oid( request, &id);
    entry = otab_lookup( &id);
    assert( entry);
    obj = &(entry->frag);
    reply = get_mpool(&message_pool);
    man_lock( obj);
    /* problem: RPC might arrive before issuing BC is handled locally,
     *          so owner field is still set to this node and the manager
     *          state won't be marshalled.
     */
    obj->fr_owner = cpu;
    mm_pack_sh_object( reply, obj);
    assert( obj->fr_manager == NULL);
    man_unlock( obj);

    rc_untagged_reply(upcall, reply);
    sys_message_clear(request);
 
    /* Synchronize with r_migrate().
     */
    sys_mutex_lock(&migrate_synch);
    if (grp_ready) {
        /* r_migrate went first, _now_ it is done */
        grp_ready = 0;
        mc_done = 1;
    } else {
        /* I got here first, r_migrate can proceed */
        rpc_ready++;
        mc_done = 0;
    }
    sys_mutex_unlock( &migrate_synch);
    if (mc_done) {
        rc_mcast_done();
    }
}


static void
r_delete_ref(int op, pan_upcall_t dummy, message_p request)
/*
 * needed to adjust reference count for "local" objects; parameters are
 * dealt with by r_exit().
 */
{
    otab_entry_p entry;
    delete_hdr_t *hdr;

    assert( op == DELETE_REF);

    hdr = sys_message_pop(request, sizeof(delete_hdr_t),
              alignof(delete_hdr_t));

    if ((entry = otab_lookup(&hdr->oid))) {
        man_dec(&entry->frag, hdr->sender, hdr->reads, hdr->writes);
    }
    rc_mcast_done();
}


static void
r_move(int op, pan_upcall_t upcall, message_p request)
/*
 * needed by man_strategy() to allocate manager at remote node.
 */
{
    fragment_t obj;
    move_hdr_t *hdr;

    assert( op == MOVE);

    hdr = sys_message_pop(request, sizeof(move_hdr_t), alignof(move_hdr_t));

    if ( hdr->dst == sys_my_pid) {
        obj.fr_fields = NULL;
        f_init( &obj, (tp_dscr *)m_getptr(hdr->type_reg), "migratory copy");
        oid_copy( &hdr->oid, &obj.fr_oid);
        mm_unpack_sh_object( request, &obj);
        /* set local access sum to a BIG value, so object stays at this node
         * even if the local process has no references to it. (Oracol)
          */
        obj.fr_info.access_sum = BIG;
	obj.fr_info.nr_accesses = BIG_INT;
    
        (void) otab_enter( &obj);
        assert( f_get_status(&obj) & RO_MANAGER);
#ifdef TRACING
        f_trc_create( &obj, obj.fr_name);
#endif
    }
    else {
        /* make sure this object gets its proper id in the object table */
	if (hdr->src == sys_my_pid) {
		sys_mutex_lock(&strategy_lock);
		sys_cond_signal(&strategy_cond);
		sys_mutex_unlock(&strategy_lock);
	}
        (void) otab_find_enter(&hdr->oid);
    }
    rc_mcast_done();
}
