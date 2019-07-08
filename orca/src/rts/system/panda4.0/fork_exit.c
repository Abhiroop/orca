/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/*********	 DoFork

forker:	Marshall value parameters
	Collect object ids of shared parameters
	Broadcast FORK message
	Wait for acknowledge of grp_listener

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

#include <assert.h>

#include "pan_sys.h"
#include "pan_align.h"
#include "pan_util.h"

#include "continuation.h"
#include "fork_exit.h"
#include "fragment.h"
#include "manager.h"
#include "msg_marshall.h"
#include "rts_comm.h"
#include "obj_tab.h"
#include "process.h"
#include "interface.h"
#include "rts_globals.h"
#include "rts_internals.h"

/*
 * Header used in FORK messages.
 */
typedef struct fork_hdr {
    int f_src;     /* forking platform */
    int f_dst;     /* destination platform (forkee) */
    int f_pdescr;  /* process descriptor index */
    int f_tdescr;  /* parameter descriptor index (dummy fork only) */
} fork_hdr_t, *fork_hdr_p;

typedef struct exit_hdr {
    int prc_reg;
    int sender;
    pan_iovec_p iov;
    void *proto;
    int n_args;
} exit_hdr_t, *exit_hdr_p;

typedef struct fetch_hdr {
    int found;
    int missing;
} fetch_hdr_t, *fetch_hdr_p;

/* Continuation used to block rpc thread that fetches shared objects.
 */
struct rfc_cont {
    int   upcall;
    void *request;
    void *proto;
};

/* Continuation used to fetch objects in a "safe" thread (i.e. that may block).
 */
struct fork_cont {
    prc_dscr *proc_descr;
    void **argv;
    int src;
    int typereg;
};


/* Pack request info for missing objects
 */

typedef struct OBJ_REQ_INFO {
    int		oid;
    int		missing;
} obj_req_info_t, *obj_req_info_p;


/* Message handlers.
 */
static int r_fork(int op, int upcall, pan_msg_p request, void *proto);
static int r_exit(int op, int upcall, pan_msg_p request, void *proto);
static int r_fetch(int op, int upcall, pan_msg_p request, void *proto);

static int fork_handle;
static int exit_handle;
static int fetch_handle;

static pan_cond_p fork_done;
static int fork_count, fork_id;
static int grp_ready_flag = 0;
static cont_queue_t fetch_cont;  /* holds blocked RPC thread if any */

static pan_cond_p all_done;
static int all_finished = 0;

static int nr_processes = 0;

static int fork_proto_start;
static int fork_proto_top;
#define fork_hdr(p)	((fork_hdr_p)((char *) (p) + fork_proto_start))
static int exit_proto_start;
static int exit_proto_top;
#define exit_hdr(p)	((exit_hdr_p)((char *) (p) + exit_proto_start))
static int fetch_proto_start;
static int fetch_proto_top;
#define fetch_hdr(p)	((fetch_hdr_p)((char *) (p) + fetch_proto_start))

/****************************************************************************/

/* We declare a dummy process descriptor for a process that takes
 * one shared object parameter. We leave one hole in the parameter
 * descriptor, for the parameter type.
 */

static par_dscr dummy_param_descr[] = {
    {
	SHARED,			/* it's a shared parameter */
	0			/* hole for type descr pointer!! */
    }
};

static tp_dscr dummy_fun_descr = {
    FUNCTION,
    sizeof(int),		/* function registration id */
    0,				/* no flags set */
    0,				/* void function result type */
    1,				/* 1 parameter */
    dummy_param_descr		/* parameter descriptors */
};

static sh_args dummy_scores_descr[] = {  /* shared arg. scores */
    {0, 0, 0},
    {0, 0, 0}
};

static int
sz_arg_rts_proc(void **argv)
{
    return 0;
}

static pan_iovec_p
ma_arg_rts_proc(pan_iovec_p p, void **argv)
{
    return p;
}

static void
um_arg_rts_proc(void *p, void **ap)
{
}

prc_dscr rts_proc_descr = {
    0,			/* no pointer to dummy process's wrapper function */
    &dummy_fun_descr,	/* function type descriptor */
    0,			/* hole for process registration number */
    dummy_scores_descr,	/* scores for shared object parameters */
    "RTS dummy process", /* tracing name */
    &sz_arg_rts_proc,	/* byte count routine */
    &ma_arg_rts_proc,	/* single-argument marshalling routine */
    &um_arg_rts_proc,	/* single-argument unmarshalling routine */
};

/****************************************************************************/

void
fe_start( void)
{
    fork_handle  = rc_export(r_fork);
    exit_handle  = rc_export(r_exit);
    fetch_handle = rc_export(r_fetch);

    cont_init(&fetch_cont);
    all_done  = rts_cond_create();
    fork_done = rts_cond_create();

    rts_proc_descr.prc_registration = 
      m_ptrregister((void *)&rts_proc_descr);
    fork_proto_start = align_to(rts_mcast_proto_top, fork_hdr_t);
    fork_proto_top = fork_proto_start + sizeof(fork_hdr_t);
    exit_proto_start = align_to(rts_mcast_proto_top, exit_hdr_t);
    exit_proto_top = exit_proto_start + sizeof(exit_hdr_t);
    fetch_proto_start = align_to(rts_rpc_proto_top, fetch_hdr_t);
    fetch_proto_top = fetch_proto_start + sizeof(fetch_hdr_t);
}

void
fe_end( void)
{
    rts_cond_clear(fork_done);
    rts_cond_clear(all_done);
    cont_clear(&fetch_cont);
}

/****************************************************************************/

static void
tell_fork_done(void)
{
    assert(!rts_trylock());

    fork_count++;
    pan_cond_signal(fork_done);
}

static void
wait_fork_done(int id)
{
    assert(!rts_trylock());
    while (fork_count < id) {
	pan_cond_wait(fork_done);
    }
}

/****************************************************************************/

void
DoExit(prc_dscr *proc_descr, void **argv)
{
    int nparams      = td_nparams(proc_descr->prc_func);
    par_dscr *params = td_params(proc_descr->prc_func);
    int		len;
    pan_iovec_p	iov;
    int	       *args;
    int		n_args = 0;
    exit_hdr_p	hdr;
    int		i;
    void       *proto;

    rts_lock();

    nparams = td_nparams(proc_descr->prc_func);
    params  = td_params(proc_descr->prc_func);
    iov = m_malloc(sizeof(pan_iovec_t));

    /* Count object ids of objects that were passed to the
     * the exiting process when it was forked.
     */
    for (i = 0; i < nparams; i++) {	/* right order for r_exit() !! */
	if (params[i].par_descr->td_type == ARRAY &&
	    params[i].par_mode == SHARED) {
	    t_array *a = argv[i];

	    n_args++;
	    n_args += a->a_sz;
	}
	else if (params[i].par_mode == SHARED) {
#ifdef DATA_PARALLEL
	    if (params[i].par_descr->td_flags & PARTITIONED) continue;
#endif
	    n_args++;
	}
    }

    args = m_malloc(n_args * sizeof(int));
    iov->data = args;
    iov->len  = n_args * sizeof(int);
    proto = rc_proto_create();

    /* Marshall object ids of objects that were passed to the
     * the exiting process when it was forked.
     */
    for (i = 0; i < nparams; i++) {	/* right order for r_exit() !! */
	if (params[i].par_descr->td_type == ARRAY &&
	    params[i].par_mode == SHARED) {
	    t_array *a = argv[i];
	    t_object *obj = ((t_object *)a->a_data) + a->a_offset;
	    int j;

	    *(args++) = a->a_sz;
	    for (j = 0; j < a->a_sz; j++) {
		*(args++) = ((fragment_p)obj)->fr_oid;
		obj++;
	    }
	}
	else if (params[i].par_mode == SHARED) {
#ifdef DATA_PARALLEL
	    if (params[i].par_descr->td_flags & PARTITIONED) continue;
#endif
	    *(args++) = ((fragment_p)argv[i])->fr_oid;
	}
    }

    hdr = exit_hdr(proto);
    hdr->prc_reg = proc_descr->prc_registration;
    hdr->sender  = rts_my_pid;
    hdr->iov     = iov;
    hdr->proto	 = proto;
    hdr->n_args  = n_args;
    rc_mcast(exit_handle, iov, 1, proto, exit_proto_top);

    rts_unlock();
}

static int
r_exit(int op, int dummy, pan_msg_p request, void *proto)
{
    int nparams, i, oid;
    par_dscr *params;
    fragment_p obj;
    prc_dscr *descr;
    exit_hdr_p	hdr;
    int		n_args = 0;
    int	       *arg, *argv;

    hdr = exit_hdr(proto);

    descr = (prc_dscr *)m_getptr(hdr->prc_reg);
    nparams = td_nparams(descr->prc_func);
    params = td_params(descr->prc_func);

    if (hdr->sender == rts_my_pid) {
	argv = arg = hdr->iov->data;
    } else {
	argv = arg = m_malloc(hdr->n_args * sizeof(int));
	rts_unlock();
	pan_msg_consume(request, arg, hdr->n_args * sizeof(int));
	rts_lock();
    }

    /*
     * Update reference counts for shared objects that were
     * passed to this process as a parameter.
     */

    for ( i=0; i < nparams; i++) {
	if (params[i].par_descr->td_type == ARRAY &&
	    params[i].par_mode == SHARED) {
	    int sz, j;

	    sz = *(arg++);
	    for (j = 0; j < sz; j++) {    /* walk array of shared objects */
		oid = *(arg++);
		if ((obj = otab_lookup(oid))) {
		    man_dec(obj, hdr->sender, (float) 0, (float) 0);
		}
	    }
	} else if (params[i].par_mode == SHARED) {
#ifdef DATA_PARALLEL
	    if (params[i].par_descr->td_flags & PARTITIONED) continue;
#endif
	    oid = *(arg++);
	    if ((obj = otab_lookup(oid))) {   /* single shared object */
		float reads, writes;

		reads = (descr->prc_shargs[i].arg_score +
			 descr->prc_shargs[i].arg_naccess)/2;
		writes= (descr->prc_shargs[i].arg_naccess-
			 descr->prc_shargs[i].arg_score)/2;
		man_dec(obj, hdr->sender, reads, writes);
	    }
	}
    }


    m_free(argv);

    if (hdr->sender == rts_my_pid) {
	rc_proto_clear(proto);
    }

    /*
     * Check if all is over.
     */
    if (--nr_processes == 0) {
	all_finished = 1;
	pan_cond_broadcast(all_done);
    }

    rc_mcast_done();
    return 1;
}

/****************************************************************************/

void
fe_await_termination(void)
{
    assert(!rts_trylock());

    while (!all_finished) {
	pan_cond_wait(all_done);
    }
}

/****************************************************************************/

void
DoFork(int cpu, prc_dscr *proc_descr, void **argv)
{
    fork_hdr_t *hdr;
    tp_dscr *type;
    int typereg = -1, my_fork_id;
    void       *proto;
    pan_iovec_p	iov;
    int		iov_size;

    rts_lock();

    cpu += rts_base_pid;		/* Convert Orca numbering to Panda */

    /* If this is a dummy fork, then we must fill in the hole in
     * the RTS process descriptor to ensure that the marshalling
     * code works. So we lock to avoid concurrent writes to the
     * descriptor.
     */
    my_fork_id = ++fork_id;
    if (proc_descr == &rts_proc_descr) {
	type    = ((fragment_p)argv[0])->fr_type;
	typereg = td_registration(type);
	dummy_param_descr[0].par_descr = type;
    }
    proto = rc_proto_create();
    iov = mm_pack_args(&iov_size, proc_descr, argv, cpu);

    hdr = fork_hdr(proto);	/* sizeof(fork_hdr_t)); */
    hdr->f_src = rts_my_pid;
    hdr->f_dst = cpu;
    hdr->f_pdescr = proc_descr->prc_registration;
    hdr->f_tdescr = typereg;

    				/* broadcast fork message */
    rc_mcast(fork_handle, iov, iov_size, proto, fork_proto_top);
    wait_fork_done(my_fork_id);   /* await local processing */

    mm_free_iovec(iov);
    rc_proto_clear(proto);

    rts_unlock();
}

static void
prepare_install(prc_dscr *proc_descr, sh_args *arg, shared_id_p si,
		int nelem, int src, int dst)
{
    float reads, writes;
    fragment_p obj;

    if (si->si_oid == OBJ_UNKNOWN) {
	si->si_oid = otab_enter(si->si_object, src);
    }
    if ((obj = otab_lookup(si->si_oid))) {

	if (proc_descr == &rts_proc_descr) {
	    man_take_decision(obj);
	} else {
	    /*
	     * Neglect compiler info in the case of arrays of shared objects
	     */
	    reads = (nelem>1 ? 0 : arg->arg_score   +  arg->arg_naccess);
	    writes= (nelem>1 ? 0 : arg->arg_naccess -  arg->arg_score);

	    man_inc(obj, src, dst, reads, writes);
	}
    }
}

static void
notify_managers(prc_dscr *proc_descr, void **argv, int src, int dst)
{
    int nparams      = td_nparams(proc_descr->prc_func);
    par_dscr *params = td_params(proc_descr->prc_func);
    shared_id_p si;
    t_array *a;
    int i, j;

    for (i = 0; i < nparams; i++) {
	if (params[i].par_descr->td_type == ARRAY &&
	    params[i].par_mode == SHARED) {

	    a = argv[i];
	    si = ((shared_id_p) a->a_data) + a->a_offset;

	    for (j = 0; j < a->a_sz; j++) {
		prepare_install(proc_descr, &(proc_descr->prc_shargs[i]),
				si, a->a_sz, src, dst);
		si++;
	    }
	} else if (params[i].par_mode == SHARED) {
#ifdef DATA_PARALLEL
	    if (params[i].par_descr->td_flags & PARTITIONED) continue;
#endif
	    si = (shared_id_p)(argv[i]);
	    prepare_install(proc_descr, &(proc_descr->prc_shargs[i]),
			    si, 1, src, dst);
	}
    }
}

static void
hand_out_shared_objects(prc_dscr *proc_descr, void **argv)
{
    int nparams      = td_nparams(proc_descr->prc_func);
    par_dscr *params = td_params(proc_descr->prc_func);
    int i;

    for (i = 0; i < nparams; i++) {
	if (params[i].par_mode == SHARED) {
#ifdef DATA_PARALLEL
	    if (params[i].par_descr->td_flags & PARTITIONED) continue;
#endif
	    /*
	     * Synchronise with RPC thread that fetches objects.
	     */
	    grp_ready_flag++;
	    if (cont_pending(&fetch_cont)) { /* RPC first */
		cont_resume(&fetch_cont);
	    }
	    return;
	}
    }
    rc_mcast_done();
    tell_fork_done();
}

static void
fetch_shared_objects(prc_dscr *proc_descr, void **argv, int cpu, int typereg)
{
    int nparams = td_nparams(proc_descr->prc_func);
    par_dscr *params = td_params(proc_descr->prc_func);
    int i, j, found, missing;
    pan_msg_p	reply;
    void       *reply_proto;
    struct info {
	struct type_descr *descr;
	int oid;
	float n_accesses;
    } *info;
    shared_id_p si;
    pan_iovec_t	iov;
    void       *proto;
    obj_req_info_p	req;

    if (proc_descr == &rts_proc_descr) {
	/* Hack: temporarily fill in the hole in the parameter
	 * type descriptor, using the type that has been sent to us.
	 */
	dummy_param_descr[0].par_descr = (tp_dscr *)m_getptr(typereg);
    }

    			/* Count # object requests to pack */
    found = 0;
    for (i = 0; i < nparams; i++) {
	if (params[i].par_descr->td_type == ARRAY &&
	    params[i].par_mode == SHARED) {
	    t_array *a = argv[i];

	    found += a->a_sz;
	} else if (params[i].par_mode == SHARED) {
#ifdef DATA_PARALLEL
	    if (params[i].par_descr->td_flags & PARTITIONED) continue;
#endif
	    found++;
	}
    }

    req = m_malloc(found * sizeof(obj_req_info_t));
    info = m_malloc(found * sizeof(struct info));

    			/* Pack object requests */
    found = 0;
    missing = 0;
    for (i = 0; i < nparams; i++) {
	if (params[i].par_descr->td_type == ARRAY &&
	    params[i].par_mode == SHARED) {
	    t_array *a = argv[i];
	    si = ((shared_id_p)(a->a_data)) + a->a_offset;

	    for (j = 0; j < a->a_sz; j++) {
		assert(si->si_oid != OBJ_UNKNOWN);
		req[found].oid = si->si_oid;
		if (!otab_lookup(si->si_oid)) {
		    /* missing = true */
		    req[found].missing = 1;
		    info[missing].descr = td_elemdscr(params[i].par_descr);
		    info[missing].oid = si->si_oid;
		    info[missing].n_accesses = 0;
		    missing++;
		} else {
		    /* missing = false */
		    req[found].missing = 0;
		}
		si++;
		found++;
	    }
	} else if (params[i].par_mode == SHARED) {
#ifdef DATA_PARALLEL
	    if (params[i].par_descr->td_flags & PARTITIONED) continue;
#endif
	    si = (shared_id_p)(argv[i]);
	    assert(si->si_oid != OBJ_UNKNOWN);
	    req[found].oid = si->si_oid;
	    if (!otab_lookup(si->si_oid)) {
		/* missing = true */
		req[found].missing = 1;
		info[missing].descr = params[i].par_descr;
		assert(si->si_oid != OBJ_UNKNOWN);
		info[missing].oid = si->si_oid;
		info[missing].n_accesses = proc_descr->prc_shargs[i].arg_naccess;
		missing++;
	    } else {
		/* missing = false */
		req[found].missing = 0;
	    }
	    found++;
	}
    }

    if (found > 0) {
	fetch_hdr_p hdr;

	proto = rc_proto_create();
	hdr = fetch_hdr(proto);
	hdr->found = found;
	hdr->missing = missing;

	/* go and get missing shared parameters */
	iov.data = req;
	iov.len  = found * sizeof(obj_req_info_t);
	rc_rpc(cpu, fetch_handle, &iov, 1, proto, fetch_proto_top,
	       &reply, &reply_proto);

	/* unmarshall shared parameters and install fragments */
	for (i = 0; i < missing; i++) {
	    fragment_t frag;
	    frag.fr_fields = NULL;
	    f_init(&frag, info[i].descr, "migratory copy");
	    frag.fr_oid = info[i].oid;
	    frag.fr_info.access_sum = info[i].n_accesses;
	    assert(frag.fr_oid != OBJ_UNKNOWN);
	    rts_unlock();
	    mm_unpack_sh_object(reply, &frag);
	    rts_lock();
	    assert(frag.fr_oid != OBJ_UNKNOWN);
#ifdef TRACING
	    f_trc_create(&frag, frag.fr_name);
#endif
	    otab_install(&frag);  /* install missing object info */
	}
	rts_unlock();
	pan_msg_clear(reply);
	rts_lock();
	rc_proto_clear(proto);
    }
    m_free(req);
    m_free(info);
}

static void
locate_shared_objects(prc_dscr *proc_descr, void **argv)
{
    int nparams      = td_nparams(proc_descr->prc_func);
    par_dscr *params = td_params(proc_descr->prc_func);
    fragment_p obj;
    shared_id_p si; 
    int i, j;

    /* Replace object ids in argv with object pointers. */

    for (i = 0; i < nparams; i++) {
	if (params[i].par_descr->td_type == ARRAY &&
	    params[i].par_mode == SHARED) {
	    t_array *a = argv[i];
	    t_object *obj_arr = m_malloc(a->a_sz * sizeof(t_object));
	    si = ((shared_id_p) (a->a_data)) + a->a_offset;

	    /* convert array of pointers into array of objects */
	    a->a_data = (char *) (obj_arr - a->a_offset);
	    for (j = 0; j < a->a_sz; j++) {
		obj = otab_lookup(si->si_oid);
		assert(obj);
		obj_arr[j] = *obj;
		si++;
	    }
	    m_free(si - a->a_sz);
	} else if (params[i].par_mode == SHARED) {
#ifdef DATA_PARALLEL
	    if (params[i].par_descr->td_flags & PARTITIONED) continue;
#endif
	    si = (shared_id_p)argv[i];
	    obj = otab_lookup(si->si_oid);
	    assert(obj);
	    argv[i] = obj;	
#ifdef RTS_VERBOSE
	    printf("%d) located %s, fields = %p\n", rts_my_pid, obj->fr_name, obj->fr_fields);
#endif
	}
    }
}

static int
r_fork_cont(void *state)
{
    struct fork_cont *fc = (struct fork_cont *)state;

    fetch_shared_objects(fc->proc_descr, fc->argv, fc->src, fc->typereg);
    locate_shared_objects(fc->proc_descr, fc->argv);

    /*
     * First, increment seqno, then start new process to avoid consistency
     * problems with RPCs tagged with incorrect sequence numbers.
     */
    rc_mcast_done();
    if (fc->proc_descr != &rts_proc_descr) {
	p_create_process(fc->proc_descr, fc->argv);
    }

    return CONT_NEXT;
}

static int
r_fork(int op, int dummy, pan_msg_p request, void *proto)
{
    int src, dst;
    prc_dscr *descr;
    void **argv;
    fork_hdr_t *hdr;
    struct fork_cont *fc;
    int ftdescr;

    hdr = fork_hdr(proto);		/* sizeof(fork_hdr_t)); */

    src  = hdr->f_src;
    dst  = hdr->f_dst;
    descr = (prc_dscr *)m_getptr(hdr->f_pdescr);
    ftdescr = hdr->f_tdescr;

    if (descr == &rts_proc_descr) {
	/*
	 * Hack: temporarily fill in the hole in the parameter
	 * type descriptor, using the type that has been sent to us.
	 */
	dummy_param_descr[0].par_descr = (tp_dscr *)m_getptr(ftdescr);
    } else {
	nr_processes++;		/* Update global process count. */
    }

    rts_unlock();
    mm_unpack_args(request, descr, &argv, (rts_my_pid == dst));
    rts_lock();

    /*
     * Notify_managers() has the important side-effect of allocating
     * object table entries for all shared objects passed in the FORK
     * call. This must be done on _all_ platforms in order to keep the
     * table identical on all platforms.
     */
    notify_managers(descr, argv, src, dst);

    if (dst == rts_my_pid) {
	if (src == rts_my_pid) {
	    locate_shared_objects(descr, argv);

	    /* Increment seqno before starting local process */
	    rc_mcast_done();
	    tell_fork_done();
	    if (descr != &rts_proc_descr) {
		p_create_process(descr, argv);
	    }
	} else {
	    /* RPC in upcall; hand to separate thread! */
	    fc = cont_alloc(&cont_immediate_queue, sizeof(*fc), r_fork_cont);
	    fc->proc_descr = descr;
	    fc->argv = argv;
	    fc->src = src;
	    fc->typereg = ftdescr;
	    cont_save(fc, 0);
	    return;
	}
    } else if (rts_my_pid == src) {
	/*
	 * Hand_out_shared_objects does rc_mcast_done() itself
	 * and calls tell_fork_done() too.
	 */
	hand_out_shared_objects(descr, argv);
     /* space leak: should clean up 'argv'!! */
    } else {
	rc_mcast_done();
    }
    return 1;
}

void
clean_r_fetch(rc_msg_p msg)
{
    int	i = 0;
    while (i < msg->iov_size) {
	i = mm_clean_sh_msg(msg->iov, i);
    }
    rts_lock();
    mm_free_iovec(msg->iov);
    rc_proto_clear(msg->proto);

/* The following lines are moved to here because the r_fetch reply is now
   asynchronous.
*/
    grp_ready_flag = 0;
    /*
     * Tell grp_listener and forker threads to continue
     */
    rc_mcast_done();
    tell_fork_done();

    rts_unlock();
    rc_msg_clear(msg);
}

static void
do_r_fetch(int upcall, pan_msg_p request, void *proto)
{
    int i, nparams;
    fragment_p obj;
    pan_iovec_p	reply;
    pan_iovec_p	iov;
    void	*rep_proto;
    int		rep_size;
    fetch_hdr_p	hdr;
    obj_req_info_t	*req;
    int		missing;
    rc_msg_p	msg;

    /* pack missing parameters */
    hdr = fetch_hdr(proto);
    nparams = hdr->found;
    missing = hdr->missing;
    rep_size = 0;
    iov = mm_get_iovec(rep_size);
    rep_proto = rc_proto_create();
    req = m_malloc(nparams * sizeof(obj_req_info_t));
    rts_unlock();
    pan_msg_consume(request, req, nparams * sizeof(obj_req_info_t));
    rts_lock();
    for (i = 0; i < nparams; i++) {
	obj = otab_lookup(req[i].oid);
	assert(obj);
	assert(obj->fr_oid != OBJ_UNKNOWN);
	if (req[i].missing) {
	    mm_pack_sh_object(&iov, &rep_size, obj);
	}
	rts_man_check(obj);
    }
    m_free(req);
    msg = rc_msg_create();
    msg->iov = iov;
    msg->iov_size = rep_size;
    msg->proto = rep_proto;
    msg->proto_size = rts_reply_proto_top;
    msg->clearfunc = clean_r_fetch;
    rc_untagged_reply(upcall, msg);
}

static int
r_fetch_cont(void *state)
{
    struct rfc_cont *cont = (struct rfc_cont *)state;

    if (grp_ready_flag) {
	do_r_fetch(cont->upcall, cont->request, cont->proto);
	return CONT_NEXT;
    }
    return CONT_KEEP;
}

static int
r_fetch(int op, int upcall, pan_msg_p request, void *proto)
{
    struct rfc_cont *rfc;

    /*
     * Need to wait for the group listener to install "new" objects in the
     * object table, so create a continuation if this RPC is first.
     */
    if (grp_ready_flag == 0) {
	rfc = (struct rfc_cont *)cont_alloc(&fetch_cont, sizeof(*rfc),
					    r_fetch_cont);
	rfc->upcall  = upcall;
	rfc->request = request;
	rfc->proto   = proto;
	cont_save(rfc, use_threads);
	return 0;
    }

    do_r_fetch( upcall, request, proto);
    return 1;
}
