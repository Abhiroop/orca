/*********	 DoFork

forker:	marshall value parameters
	collect object ids of shared parameters
	Broadcast FORK message
	NO need to wait for acknowledge of grp_listener

forkee (grp_listener at remote cpu):
	notify manager of new scores
	RPC to forking processor to get missing shared parameters
	unmarshall shared parameters from reply; install objects
	unmarshall value parameters from BC message
	start new_thread

RPC server:
	(wait for seqno for consistency)
	marshall requested parameters
	signal listener
	return reply

grp_listener:
	notify manager of new scores
	on the source CPU, wait for RPC to finish

*********/

#include "panda/panda.h"
#include "continuation.h"
#include "fork_exit.h"
#include "fragment.h"
#include "manager.h"
#include "msg_marshall.h"
#include "rts_comm.h"
#include "rts_util.h"
#include "obj_tab.h"
#include "process.h"
#include "interface.h"

/*
 * Header used in FORK messages.
 */
typedef struct fork_hdr {
    int f_src;     /* forking platform */
    int f_dst;     /* destination platform (forkee) */
    int f_pdescr;  /* process descriptor index */
} fork_hdr_t;


/* Continuation used to block rpc thread that fetches shared objects.
 */
struct rfc_cont {
	pan_upcall_t upcall;
	message_p request;
};

/* Continuation used to fetch objects in a "safe" thread (i.e. that may block).
 */
struct fork_cont {
    prc_dscr *descr;
    void **argv;
    int src;
};

/* Message handlers.
 */
static void r_fork(int op, pan_upcall_t upcall, message_p request);
static void r_exit(int op, pan_upcall_t upcall, message_p request);
static void r_fetch(int op, pan_upcall_t upcall, message_p request);

static operation_p fe_operations[] = { r_fork, r_exit, r_fetch};
#define FORK	0
#define EXIT	1
#define FETCH	2

extern mpool_t message_pool;        /* Global message pool */

static mutex_t fe_lock;
static int grp_ready_flag = 0;
static cont_queue_t fetch_cont;  /* holds blocked RPC thread if any */
static int fe_server;

static mutex_t termination_lock;
static cond_t all_done;
static int all_finished = 0;

static int nr_processes = 0;


/******************************************************************************/


void
fe_start( void)
{
	fe_server = rc_export( "fe_server", 3, fe_operations);
	sys_mutex_init( &fe_lock);
    	cont_init(&fetch_cont, &fe_lock);
	sys_mutex_init( &termination_lock);
	sys_cond_init( &all_done);
}


void
fe_end( void)
{
    sys_cond_clear( &all_done);
	sys_mutex_clear( &termination_lock);
    cont_clear(&fetch_cont);
	sys_mutex_clear( &fe_lock);
}


/******************************************************************************/


void
DoExit(prc_dscr *descr, void **argv)
{
	int nparams      = td_nparams(descr->prc_func);
	par_dscr *params = td_params(descr->prc_func);
	message_p msg    = (message_p) grp_message_init();
	int i;

	for ( i=nparams; i-- > 0; )	/* right order for r_exit() !! */
		if (params[i].par_descr->td_type == ARRAY &&
		    params[i].par_mode == SHARED) {
 			t_array *a = argv[i];
			t_object *obj = ((t_object *) a->a_data) + a->a_offset;
                        int j;

                        for (j = 0; j < a->a_sz; j++) {
				ru_push_oid( msg, &((fragment_p) obj)->fr_oid);
				obj++;
                        }
			tm_push_int( msg, a->a_sz);
		}
		else if (params[i].par_mode == SHARED) {
			ru_push_oid( msg, &(((fragment_p) argv[i])->fr_oid));
		}

	tm_push_tid( msg, sys_thread_self());
	tm_push_int( msg, descr->prc_registration);
	tm_push_int( msg, sys_my_pid);
	rc_mcast(fe_server, EXIT, msg);
}


static void
r_exit(int op, pan_upcall_t dummy, message_p request)
{
    int nparams, i, dptr, cpu;
    par_dscr *params;
    oid_t id;
    otab_entry_p entry;
    prc_dscr *descr;
    thread_t tid;

    tm_pop_int( request, &cpu);
    tm_pop_int( request, &dptr);
    tm_pop_tid( request, &tid);

    descr = (prc_dscr *)m_getptr( dptr);
    nparams = td_nparams(descr->prc_func);
    params = td_params(descr->prc_func);
    for ( i=0; i < nparams; i++) {
	if (params[i].par_descr->td_type == ARRAY &&
	    params[i].par_mode == SHARED) {
	    int sz, j;

	    tm_pop_int( request, &sz);
	    for (j = 0; j < sz; j++) {
		ru_pop_oid( request, &id);
		if ((entry = otab_lookup( &id))) {
		    fragment_p obj = &(entry->frag);
		    double reads, writes;
	
		    reads = (descr->prc_shargs[i].arg_score +
			     descr->prc_shargs[i].arg_naccess)/(2*sz);
		    writes= (descr->prc_shargs[i].arg_naccess-
			     descr->prc_shargs[i].arg_score)/(2*sz);
		    man_dec( obj, cpu, reads, writes);
		}
	    }
	} else if (params[i].par_mode == SHARED) {
	    ru_pop_oid( request, &id);
	    if ((entry = otab_lookup( &id))) {
		fragment_p obj = &(entry->frag);
		double reads, writes;

		reads = (descr->prc_shargs[i].arg_score +
			 descr->prc_shargs[i].arg_naccess)/2;
		writes= (descr->prc_shargs[i].arg_naccess-
			 descr->prc_shargs[i].arg_score)/2;
		man_dec( obj, cpu, reads, writes);
	    }
	}
    }
		
    /* cleanup terminating thread */
    if (cpu == sys_my_pid) {       
	sys_thread_join(&tid);
    }
	
    /* check if all is over */
    sys_mutex_lock(&termination_lock);
    if (--nr_processes == 0) {
	all_finished = 1;
	sys_cond_broadcast(&all_done);
    }
    sys_mutex_unlock(&termination_lock);
    rc_mcast_done();
}


/******************************************************************************/

void
fe_await_termination(void)
{
        sys_mutex_lock(&termination_lock);
        while (!all_finished) {
	        sys_cond_wait(&all_done, &termination_lock);
	}
	sys_mutex_unlock(&termination_lock);
}


/******************************************************************************/


void
DoFork(int cpu, prc_dscr *descr, void **argv)
{
    fork_hdr_t *hdr;
    message_p msg = grp_message_init();
    extern int rts_base_pid;
    
    cpu += rts_base_pid;		/* Convert Orca numbering to Panda */
    assert( 1 <= cpu && cpu <= sys_nr_platforms);
    
    mm_pack_args( msg, descr, argv, cpu);
    hdr = (fork_hdr_t *)sys_message_push(msg, sizeof(fork_hdr_t),
					 alignof(fork_hdr_t));
    hdr->f_src = sys_my_pid;
    hdr->f_dst = cpu;
    hdr->f_pdescr = descr->prc_registration;

    /* NO need to wait for acknowledge of grp_listener.
     */
    rc_mcast(fe_server, FORK, msg);
}


static void
notify_managers( prc_dscr *descr, void **argv, int src, int dst)
{
    int nparams = td_nparams(descr->prc_func);
    par_dscr *params = td_params(descr->prc_func);
    int i;
    otab_entry_p entry;
    
    for (i = 0; i < nparams; i++) {
	if (params[i].par_descr->td_type == ARRAY &&
	    params[i].par_mode == SHARED) {
	    t_array *a = argv[i];
	    oid_p ptr = ((oid_p) a->a_data) + a->a_offset;
	    int j;
	    for (j = 0; j < a->a_sz; j++) {
		if ((entry = otab_find_enter(ptr))) {
		    fragment_p obj = &(entry->frag);
		    double reads, writes;
	
		    reads = (descr->prc_shargs[i].arg_score +
			     descr->prc_shargs[i].arg_naccess)/(2*a->a_sz);
		    writes= (descr->prc_shargs[i].arg_naccess-
			     descr->prc_shargs[i].arg_score)/(2*a->a_sz);
		    man_inc(obj, src, dst, reads, writes);
		}
		ptr++;
	    }
	} else if (params[i].par_mode == SHARED) {
	    if ((entry = otab_find_enter((oid_p)argv[i]))) {
		fragment_p obj = &(entry->frag);
		double reads, writes;

		reads = (descr->prc_shargs[i].arg_score +
			 descr->prc_shargs[i].arg_naccess)/2;
		writes= (descr->prc_shargs[i].arg_naccess-
			 descr->prc_shargs[i].arg_score)/2;

		man_inc(obj, src, dst, reads, writes);
	    }
	}
    }
}


static void
hand_out_shared_objects( prc_dscr *descr, void **argv)
{
    int nparams = td_nparams(descr->prc_func);
    par_dscr *params = td_params(descr->prc_func);
    int i;
    
    for (i = 0; i < nparams; i++) {
	if (params[i].par_mode == SHARED) {
	    /* synchronise with RPC thread that fetches objects */

	    sys_mutex_lock(&fe_lock);
	    grp_ready_flag++;
	    if (cont_pending(&fetch_cont)) { /* RPC first */
		cont_resume(&fetch_cont);
	    }
	    sys_mutex_unlock( &fe_lock);
	    return;
	}
    }
    rc_mcast_done();
}


static void
fetch_shared_objects( prc_dscr *descr, void **argv, int cpu)
{
    int nparams = td_nparams(descr->prc_func);
    par_dscr *params = td_params(descr->prc_func);
    int i, found, missing;
    message_p request, reply;
    struct info {
	struct type_descr *descr;
	oid_p oid;
	double n_accesses;
    } *info;
    int max_info = 100;
    
    info = (struct info *) sys_malloc( max_info * sizeof(struct info));
    request = get_mpool(&message_pool);
    
    missing = 0;
    found = 0;
    for (i = 0; i < nparams; i++) {
	if (params[i].par_descr->td_type == ARRAY &&
	    params[i].par_mode == SHARED) {
	    t_array *a = argv[i];
	    oid_p ptr = ((oid_p) (a->a_data)) + a->a_offset;
	    int j;
	  
	    for (j = 0; j < a->a_sz; j++) {
		found++;
		ru_push_oid(request, ptr);
		if (!otab_lookup(ptr)) {
		    tm_push_int(request, 1);
		    info[missing].descr = td_elemdscr(params[i].par_descr);
		    info[missing].oid = ptr;
		    info[missing].n_accesses = descr->prc_shargs[i].arg_naccess / a->a_sz; 
		    missing++;
		    if (missing == max_info) {
			max_info *= 2;
			info = (struct info *) sys_realloc(info, max_info * sizeof(struct info));
		    }
		} else {
		    tm_push_int(request, 0);
		}
		ptr++;
	    }
	} else if (params[i].par_mode == SHARED) {
	    found++;
	    ru_push_oid(request, (oid_p) argv[i]);
	    if (!otab_lookup((oid_p)argv[i])) {
		tm_push_int(request, 1);
		info[missing].descr = params[i].par_descr;
		info[missing].oid = (oid_p) argv[i];
		info[missing].n_accesses = descr->prc_shargs[i].arg_naccess;
		missing++;
		if (missing == max_info) {
		    max_info *= 2;
		    info = (struct info *) sys_realloc(info, max_info * sizeof(struct info));
		}
	    } else {
		tm_push_int(request, 0);
	    }
	}
    }

    if (found > 0) {
	tm_push_int(request, found);

	/* go and get missing shared parameters */
	rc_rpc( cpu, fe_server, FETCH, request, &reply);

	/* unmarshall shared parameters and install fragments */
	for ( i=0; i < missing; i++) {
	    fragment_t frag;
	    frag.fr_fields = NULL;
	    f_init(&frag, info[i].descr, "migratory copy");
	    oid_copy( info[i].oid, &frag.fr_oid);
	    frag.fr_info.access_sum = info[i].n_accesses;
	    mm_unpack_sh_object( reply, &frag);
#ifdef TRACING
	    f_trc_create( &frag, frag.fr_name);
#endif
	    (void)otab_enter(&frag);
	}
	sys_message_clear( reply);
    }
    sys_message_clear(request);
    sys_free( info);
}


static void
locate_shared_objects( prc_dscr *descr, void **argv)
{
    int nparams = td_nparams(descr->prc_func);
    par_dscr *params = td_params(descr->prc_func);
    int i;
    otab_entry_p entry;
    
    for ( i=0; i < nparams; i++) {
	if (params[i].par_descr->td_type == ARRAY &&
	    params[i].par_mode == SHARED) {
	    t_array *a = argv[i];
	    oid_p ptr = ((oid_p) (a->a_data)) + a->a_offset;
	    t_object *obj = sys_malloc( a->a_sz * sizeof(t_object));
	    int j;

	    /* convert array of pointers into array of objects */
	    a->a_data = (char *) (obj - a->a_offset);
	    for (j = 0; j < a->a_sz; j++) {
		entry = otab_lookup(ptr);
		assert(entry);
		*obj = entry->frag;
		ptr++;
		obj++;
	    }
	    sys_free( (void *)(ptr - a->a_sz));
	} else if (params[i].par_mode == SHARED) {
	    entry = otab_lookup((oid_p)argv[i]);
	    assert(entry);
	    argv[i] = &(entry->frag);	
	}
    }
}


static int r_fork_cont(void *state, mutex_p lock)
{
	struct fork_cont *fc = (struct fork_cont *)state;

	sys_mutex_unlock(lock);
	fetch_shared_objects( fc->descr, fc->argv, fc->src);
	locate_shared_objects( fc->descr, fc->argv);
	p_create_process( fc->descr, fc->argv);
	rc_mcast_done();
	sys_mutex_lock(lock);
	return CONT_NEXT;
}


static void
r_fork(int op, pan_upcall_t dummy, message_p request)
{
    int src, dst;
    prc_dscr *descr;
    void **argv;
    fork_hdr_t *hdr;
    struct fork_cont *fc;
    
    hdr = (fork_hdr_t *)sys_message_pop(request, sizeof(fork_hdr_t),
					alignof(fork_hdr_t));
    src  = hdr->f_src;
    dst  = hdr->f_dst;
    descr = (prc_dscr *)m_getptr(hdr->f_pdescr);
    
    mm_unpack_args(request, descr, &argv, (sys_my_pid == dst));

    /*
     * Notify_managers() has the important side-effect of allocating
     * object table entries for all shared objects passed in the FORK
     * call. This must be done on _all_ platforms in order to keep the
     * table identical on all platforms.
     */
    notify_managers(descr, argv, src, dst);

    sys_mutex_lock(&termination_lock);
    nr_processes++;
    sys_mutex_unlock(&termination_lock);

    if (dst == sys_my_pid) {
	if (src == sys_my_pid) {
	    locate_shared_objects(descr, argv);
	    p_create_process(descr, argv);
	    rc_mcast_done();
	} else {
	    sys_mutex_lock(&cont_immediate_lock);
	    fc = (struct fork_cont *)cont_alloc(&cont_immediate_queue,
						sizeof(*fc), r_fork_cont);
	    fc->descr = descr;
	    fc->argv = argv;
	    fc->src = src;
	    cont_save(fc, 0);
	    sys_mutex_unlock(&cont_immediate_lock);
	    return;
	}
    } else if (sys_my_pid == src) {
	hand_out_shared_objects(descr, argv);  /* does rc_mcast_done() itself */
     /* space leak: should clean up 'argv'!! */
    } else {
        rc_mcast_done();
    }
}


static void
do_r_fetch(pan_upcall_t upcall, message_p request)
{
    int missing, i, nparams;
    oid_t id;
    otab_entry_p entry;
    message_p reply;

    /* pack missing parameters */
    tm_pop_int(request, &nparams);
    reply = get_mpool(&message_pool);
    for (i = 0; i < nparams; i++) {
	tm_pop_int(request, &missing);
	ru_pop_oid(request, &id);
	entry = otab_lookup(&id);
	assert(entry);
	man_lock( &entry->frag);
	/* Record that another copy will be created, see man_inc() */
	entry->frag.fr_total_refs++;
	if (missing) {
	    mm_pack_sh_object( reply, &entry->frag);
	}
	man_unlock( &entry->frag);
    }
    rc_untagged_reply(upcall, reply);
    sys_message_clear(request);
	
    grp_ready_flag = 0;

    sys_mutex_unlock( &fe_lock);
    /* signal grp_listener to continue */
    rc_mcast_done();
    sys_mutex_lock( &fe_lock);
}


static int r_fetch_cont(void *state, mutex_p lock)
{
    struct rfc_cont *cont = (struct rfc_cont *)state;

    if (grp_ready_flag) {
    	do_r_fetch( cont->upcall, cont->request);
	return CONT_NEXT;
    }
    return CONT_KEEP;
}


static void
r_fetch(int op, pan_upcall_t upcall, message_p request)
{
    struct rfc_cont *rfc;

    /* Need to wait for the group listener to install "new" objects in the
     * object table, so create a continuation if rpc is first.
     */
    sys_mutex_lock( &fe_lock);
    if (grp_ready_flag == 0) {
	rfc = (struct rfc_cont *)cont_alloc(&fetch_cont, sizeof(*rfc),
					    r_fetch_cont);
	rfc->upcall  = upcall;
	rfc->request = request;
#ifdef BLOCKING_UPCALLS
	cont_save(rfc, 1);
#else
	cont_save(rfc, 0);
#endif
    	sys_mutex_unlock( &fe_lock);
	return;
    }

    do_r_fetch( upcall, request);
    sys_mutex_unlock( &fe_lock);
}
