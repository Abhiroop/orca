/* $Id: fork.c,v 1.31 1996/07/15 14:51:15 ceriel Exp $ */

#include "interface.h"
#include "amoeba.h"
#include "semaphore.h"
#include "scheduler.h"
#include "trace_rec.h"
#include "remote.h"
#include "policy.h"
#include "message.h"
#include "module/syscall.h"
#include "module/mutex.h"

extern FILE *file;
extern int this_cpu, ncpus;
extern int maxmesssize;
extern int NoForkYet;
extern int replication_policy;
extern int rts_verbose;
extern struct opstat opstat;
#ifdef CHK_FORK_MSGS
extern int chksums;
#endif

int ob_nbytes();
char *ob_marshall();
char *ob_unmarshall();

static trace_rec_t forkrec;

semaphore fork_lock;	/* Only one DoFork busy per processor */
extern semaphore allow_fork;	/* When it is our turn to fork */

void	chk_size();

#ifdef OPTIMIZED
#ifdef CHK_FORK_MSGS
char *bufs[128];
int buflens[128];
#endif

void r_object(hdr, request, reqlen)
    header *hdr;
    char *request;
    int reqlen;
{
    t_object o, *o1, *AddObject();
    char *p = request;
    int oid;
    tp_dscr *d;
    
    memcpy(&oid, p, sizeof(int));
    d = m_getptr(oid);
    p += sizeof(int);
    p = ob_unmarshall(*p, &o, d, 1);
    assert(p - request == reqlen);

    if (rts_verbose > 0)
    	fprintf(file, "%d: receive object %d\n", this_cpu, o.o_id);

    o.o_state = 0;

    o1 = AddObject(&o);
    assert(o1);

    memcpy(o1->o_fields, o.o_fields, d->td_size);
    m_free(o.o_fields);
    m_free(o.o_state);
    m_free(o.o_rtsdep);
    o1->o_state[this_cpu].o_replica = 1;

    sema_up(&o1->o_semdata);		/* we got the data */
}


static void give_copy(o, i)
    t_object *o;
    int i;
{
    char *buf = m_malloc(maxmesssize);
    char *p;
    int len;
    int replen;
    int oid = td_registration(o->o_dscr);

    assert(buf);
    p = buf;
    len = ob_nbytes(o, o->o_dscr, 1) + sizeof(int);

    chk_size(len, "migrate_obj");

    memcpy(buf, &oid, sizeof(int));
    p = ob_marshall(p, o, o->o_dscr, 1);
    assert(p - buf == len);

    rpc_doop(HAVEACOPY, i, buf, len, &replen);

    m_free(buf);
}

/* Add the object state to the fork message. First, compute the number of
 * bytes needed. Second, marshall the state. In principle, the first time we
 * install a copy on the to be forked processor. However, we can only do it, if
 * we have a copy ourselves. Register this info in the state for cpu, so
 * that the other processors know if cpu will get a copy or not.
 */
static void add_obj_state(buf, descr, argtab, mlen, cpu)
    char *buf;
    prc_dscr *descr;
    void **argtab;
    int *mlen;
    int cpu;
{
    int n = td_nparams(descr->prc_func);
    par_dscr *params = td_params(descr->prc_func);
    int size = *mlen;
    int cnt = 0;
    char *p;
    t_object *o;
    register int i;

    size += sizeof(int);
    for (i = 0; i < n; i++, params++) {
	if (params->par_mode == SHARED) {
	    if (params->par_descr->td_type == ARRAY) {
		t_array *a = argtab[i];

		assert(td_elemdscr(params->par_descr)->td_type == OBJECT);
		size += sizeof(int);
		size += a->a_sz * (sizeof(t_obj_state) * ncpus + sizeof(int));
	    }
	    else {
	    	assert(params->par_descr->td_type == OBJECT);
	    	size += sizeof(t_obj_state) * ncpus + sizeof(int);
	    }
	}
    }

    cnt = size - *mlen;

    chk_size(size, "add_object_state");

    p = buf + *mlen;
    params = td_params(descr->prc_func);
    
    memcpy(p, &cnt,sizeof(int));
    p += sizeof(int);
    for (i=0; i < n; i++, params++) {
	void *a = argtab[i];
	if (params->par_mode == SHARED) {
	    if (params->par_descr->td_type == ARRAY) {
		t_array *ap = argtab[i];
		register int j;

		o = &(((t_object *)(ap->a_data))[ap->a_offset]);
		memcpy(p, &(ap->a_sz), sizeof(int));
		p += sizeof(int);
		for (j = ap->a_sz; j > 0; j--, o++) {
		    memcpy(p, &o->o_id,sizeof(int));
		    p += sizeof(int);
	    	    assert(o->o_id != -1);
	    	    assert(o->o_state);

	    	    /* If we do not have a copy, the dest. will not get one either */
	    	    if(o->o_state[this_cpu].o_replica && 
	       	       o->o_state[cpu].o_refcnt == 0) {
		    	if (! o->o_strategy_set ||
			    o->o_replicated ||
			    o->o_owner == cpu) {
		        	o->o_state[cpu].o_replica = 1;
			}
	    	    }
	    	    memcpy(p, o->o_state, ncpus * sizeof(t_obj_state));
	    	    p += ncpus * sizeof(t_obj_state);
		}
	    }
	    else {
	    	o = (t_object *) a;
	    	memcpy(p, &o->o_id,sizeof(int));
	    	assert(o->o_id != -1);
	    	assert(o->o_state);
	    	p += sizeof(int);
	
	    	/* If we do not have a copy, the dest. will not get one either */
	    	if(o->o_state[this_cpu].o_replica && 
	       	   o->o_state[cpu].o_refcnt == 0) {
		    	if (! o->o_strategy_set ||
			    o->o_replicated ||
			    o->o_owner == cpu) {
		        	o->o_state[cpu].o_replica = 1;
			}
	    	}

	    	memcpy(p, o->o_state, ncpus * sizeof(t_obj_state));
	    	p += ncpus * sizeof(t_obj_state);
	    }
	}
    }
    *mlen = size;
}
#endif


static void add_obj_ids(buf, descr, argtab, mlen)
    char *buf;
    prc_dscr *descr;
    void **argtab;
    int *mlen;
{
    int n = td_nparams(descr->prc_func);
    par_dscr *params = td_params(descr->prc_func);
    int i;

    *mlen = 0;
    for (i = 0; i < n; i++, params++) {
	void *a = argtab[i];

	if (params->par_mode == SHARED) {
	    if (params->par_descr->td_type == ARRAY) {
		t_array *ap = a;
		t_object *o;
		register int j;

		o = &(((t_object *)(ap->a_data))[ap->a_offset]);
		for (j = ap->a_sz; j > 0; j--, o++) {
		    memcpy(buf, &o->o_id,sizeof(int));
		    buf += sizeof(int);
		    *mlen += sizeof(int);
		}
	    }
	    else {
	    	t_object *o = (t_object *) a;
	    	memcpy(buf, &o->o_id, sizeof(int));
	    	*mlen += sizeof(int);
	    	buf += sizeof(int);
	    }
	}
    }
}

void
printbuf(buf, len)
	char *buf;
	int len;
{
	register int i;
	for (i = 0; i < len; i++) {
		if (! (i % 16)) {
			if (i != 0) fprintf(file, "\n");
			fprintf(file, "%05o\t", i);
		}
		fprintf(file, "%3o ", buf[i]&0377);
	}
	fprintf(file, "\n");
}

int o_rts_nbytes(op, d)
	t_object *op;
	tp_dscr	*d;
{
	return sizeof(struct t_objrts);
}

char *o_rts_marshall(p, op, d)
	char	*p;
	t_object *op;
	tp_dscr	*d;
{

	memcpy(p, op->o_rtsdep, sizeof(struct t_objrts));
	return p + sizeof(struct t_objrts);
}

char *o_rts_unmarshall(p, op, d)
	char	*p;
	t_object *op;
	tp_dscr	*d;
{
	op->o_rtsdep = m_malloc(sizeof(struct t_objrts));
	memcpy(op->o_rtsdep, p, sizeof(struct t_objrts));
	op->o_fields = 0;
	return p + sizeof(struct t_objrts);
}

int ob_nbytes(op, d, n)
	t_object *op;
	tp_dscr	*d;
	int	n;
{
	int len = sizeof(struct t_objrts);
        if (n) len += (*(td_objinfo(d)->obj_size_obj))(op);
	return len;
}

char *ob_marshall(p, op, d, n)
	char	*p;
	t_object *op;
	tp_dscr	*d;
	int	n;
{

	memcpy(p, op->o_rtsdep, sizeof(struct t_objrts));
	p += sizeof(struct t_objrts);
	if (n) {
    		p = (*(td_objinfo(d)->obj_marshall_obj))(p, op);
	}
	return p;
}

char *ob_unmarshall(p, op, d, n)
	char	*p;
	t_object *op;
	tp_dscr	*d;
	int	n;
{
	op->o_rtsdep = m_malloc(sizeof(struct t_objrts));
	memcpy(op->o_rtsdep, p, sizeof(struct t_objrts));
	p += sizeof(struct t_objrts);
	op->o_fields = 0;
	if (n) {
		p = (*(td_objinfo(d)->obj_unmarshall_obj))(p, op);
	}
	return p;
}

/* Add the shared objects to fork message. It works in two phases. In the
 * first phase the length is computed; in the second phase the objects are
 * marshalled. If a shared object is marshalled, there are three cases to
 * be considered. 1) If the destination already shares object, only the id
 * is marshalled. 2) If the destination does not have the object, but we only
 * have the descriptor without the actual data, the remote processor will
 * only get the descriptor (the descriptor contains which processor has the
 * actual data for the object). 3) If the destination does not have the object
 * and we have a replica of the object, the complete object is marshalled.
 */
static void AddSharedObjects(margs, descr, argtab, mlen, cpu)
    char *margs;
    prc_dscr *descr;
    void **argtab;
    int *mlen;
    int cpu;
{
    int i;
    int n;		/* number of arguments to fork */
    int offset = *mlen;	/* where to start marshalling */
    t_object *o;		/* pointer to object to be marshalled */
    char *p;		/* pointer in margs */
    int has;		/* keeps track if cpu already has copy of obj */
    int with;		/* keeps track if the src has the data of obj */
    register par_dscr *params;
    
    n = td_nparams(descr->prc_func);
    params = td_params(descr->prc_func);
    for (i = 0; i < n; i++, params++) {
	void *a = argtab[i];
	if (params->par_mode == SHARED) {
	    tp_dscr *d = params->par_descr;
	    if (params->par_descr->td_type == ARRAY) {
		t_array *ap = a;
		register int j;

		d = td_elemdscr(d);
		*mlen += sizeof(int);	/* # of elements in array */
		*mlen += td_ndim(params->par_descr) * 2 * sizeof(int);
		if (ap->a_sz <= 0) {
			continue;
		}
		o = &(((t_object *)(ap->a_data))[ap->a_offset]);
		for (j = ap->a_sz; j > 0; j--, o++) {
		    o->o_shared++;
#ifdef OPTIMIZED
	    	    assert(o->o_state);
	    	    if (o->o_state[cpu].o_refcnt > 0) has = 1;
	    	    else has = 0;
	    	    if(o->o_state[this_cpu].o_replica &&
		       o->o_state[cpu].o_replica) with = 1;
	    	    else with = 0;
	    	    *mlen += sizeof(int);			/* has */
	    	    if (has) *mlen += sizeof(int); 		/* marshall obj id */
	    	    else {
		        *mlen += sizeof(int);			/* with */
			*mlen += ob_nbytes(o, d, with);
	    	    }
#else
		    *mlen += sizeof(int);			/* with */
		    if (o->o_strategy_set &&
			! o->o_replicated &&
			o->o_owner != cpu) {
			*mlen += ob_nbytes(o, d, 0);
		    }
	    	    else *mlen += ob_nbytes(o, d, 1);
#endif
		}
	    }
	    else {
	   	o = a;
	    	o->o_shared++;
#ifdef OPTIMIZED
	    	assert(o->o_state);
	    	if (o->o_state[cpu].o_refcnt > 0) has = 1;
	    	else has = 0;
	    	if(o->o_state[this_cpu].o_replica &&
		   o->o_state[cpu].o_replica) with = 1;
	    	else with = 0;
	    	*mlen += sizeof(int);			/* has */
	    	if (has) *mlen += sizeof(int); 		/* marshall obj id */
	    	else {
		    *mlen += sizeof(int);			/* with */
		    *mlen += ob_nbytes(o, d, with);
	    	}
#else
		*mlen += sizeof(int);			/* with */
		if (o->o_strategy_set &&
		    ! o->o_replicated &&
		    o->o_owner != cpu) {
		        *mlen += ob_nbytes(o, d, 0);
		}
	    	else *mlen += ob_nbytes(o, d, 1);
#endif
	    }
	}
    }
    p = &margs[offset];
    chk_size(*mlen, "AddSharedObject");
    params = td_params(descr->prc_func);
    for (i = 0; i < n; i++, params++) {
	void *a = argtab[i];
	if (params->par_mode == SHARED) {
	    tp_dscr *d = params->par_descr;
	    if (params->par_descr->td_type == ARRAY) {
		t_array *ap = a;
		register int j;

		d = td_elemdscr(d);
		memcpy(p, &(ap->a_sz), sizeof(int));	/* # of elements in array */
		p += sizeof(int);
		memcpy(p, ap->a_dims, td_ndim(params->par_descr) * 2 * sizeof(int));
		p += td_ndim(params->par_descr) * 2 * sizeof(int);
		if (ap->a_sz <= 0) {
			continue;
		}
		o = &(((t_object *)(ap->a_data))[ap->a_offset]);
		for (j = ap->a_sz; j > 0; j--, o++) {
	    	    assert(o->o_id != -1);
#ifdef OPTIMIZED
	    	    assert(o->o_state);
	    	    if (o->o_state[cpu].o_refcnt > 0) has = 1;
	    	    else has = 0;
	    	    memcpy(p, &has, sizeof(int));
	    	    p += sizeof(int);
	    	    if(has)  {
			memcpy(p, &o->o_id, sizeof(int));
			p += sizeof(int);
	    	    } else {
			if(o->o_state[this_cpu].o_replica &&
			   o->o_state[cpu].o_replica) with = 1;
			else with = 0;
			memcpy(p, &with, sizeof(int));
			p += sizeof(int);
			p = ob_marshall(p, o, d, with);
	    	    }
#else
		    if (o->o_strategy_set &&
			! o->o_replicated &&
			o->o_owner != cpu) {
			with = 0;
		    }
	    	    else with = 1;
		    memcpy(p, &with, sizeof(int));
		    p += sizeof(int);
		    p = ob_marshall(p, o, d, with);
#endif
	            if (cpu != this_cpu) mu_unlock(&o->o_mutex);
		}
	    }
	    else {
	    	o = a;
	    	assert(o->o_id != -1);
#ifdef OPTIMIZED
	    	assert(o->o_state);
	    	if (o->o_state[cpu].o_refcnt > 0) has = 1;
	    	else has = 0;
	    	memcpy(p, &has, sizeof(int));
	    	p += sizeof(int);
	    	if(has)  {
			memcpy(p, &o->o_id, sizeof(int));
			p += sizeof(int);
	    	} else {
			if(o->o_state[this_cpu].o_replica &&
			   o->o_state[cpu].o_replica) with = 1;
			else with = 0;
			memcpy(p, &with, sizeof(int));
			p += sizeof(int);
		        p = ob_marshall(p, o, d, with);
	    	}
#else
		if (o->o_strategy_set &&
		    ! o->o_replicated &&
		    o->o_owner != cpu) {
	    	    	with = 0;
		}
		else with = 1;
	    	memcpy(p, &with, sizeof(int));
	    	p += sizeof(int);
		p = ob_marshall(p, o, d, with);
#endif
	        if (cpu != this_cpu) mu_unlock(&o->o_mutex);
	    }
	}
    }
}

/* VARARGS */
void
DoFork(int cpu, prc_dscr *descr, void **argtab)
{
    int mlen = 0;			/* length of the to be marshalled msg */
    process_p me = GET_TASKDATA();

    
    put_fork_rec(&forkrec, this_cpu, cpu, descr->prc_name);
#ifdef OPTIMIZED
    if((NoForkYet && this_cpu == 0 && cpu == 0)) {
	assert(cpu == this_cpu);
	create_local_process(descr, copy_args(descr, argtab));
    } else {
	int tmp;
	NoForkYet = 0;
        sema_down(&fork_lock);
	add_obj_ids(me->buf, descr, argtab, &mlen);
	grp_update(WANT_FORK, cpu, me->buf, mlen, me);
	mlen = 0;
	add_obj_state(me->buf, descr, argtab, &mlen, cpu);
	memcpy(&(me->buf[mlen]), &(descr->prc_registration), sizeof(int));
	mlen += sizeof(int);

	tmp = (*(descr->prc_size_args))(argtab);
	chk_size(mlen+tmp, "DoFork");
	mlen = (*(descr->prc_marshall_args))(me->buf+mlen, argtab) - me->buf;
	AddSharedObjects(me->buf, descr, argtab, &mlen, cpu);
	sema_down(&allow_fork);
#ifdef CHK_FORK_MSGS
	if (chksums) {
		/* Put checksum in count field, so that we don't change the
		   size of the message.
		*/
		int c;

		if (cpu < 128) {
			if (bufs[cpu]) m_free(bufs[cpu]);
			bufs[cpu] = m_malloc(mlen);
			buflens[cpu] = mlen;
			memcpy(bufs[cpu], me->buf, mlen);
		}
		memcpy(&c, me->buf, sizeof(int));
		c |= chksum(me->buf, mlen) << 16;
		memcpy(me->buf, &c, sizeof(int));
        }
#endif
	grp_update(FORK, cpu, me->buf, mlen, me);
	sema_up(&fork_lock);
    }
#else
    /* increment reference counts of objects passed as shared parameter */
    if (cpu == this_cpu) {
	create_local_process(descr, copy_args(descr, argtab));
    } else {
	NoForkYet = 0;
        sema_down(&fork_lock);
	add_obj_ids(me->buf, descr, argtab, &mlen);
	grp_update(WANT_FORK, cpu, me->buf, mlen, me);
	mlen = (*(descr->prc_size_args))(argtab, cpu) + sizeof(int);
	memcpy(&(me->buf[0]), &(descr->prc_registration), sizeof(int));
	chk_size(mlen, "DoFork");
	mlen = (*(descr->prc_marshall_args))(me->buf+sizeof(int), argtab, cpu) - me->buf;
	AddSharedObjects(me->buf, descr, argtab, &mlen, cpu);
	/* printbuf(me->buf, mlen); */
	sema_down(&allow_fork);
	grp_update(FORK, cpu, me->buf, mlen, me);
        sema_up(&fork_lock);
    }
#endif
}

static void
get_obj(p, d, op)
    char	**p;
    tp_dscr	*d;
    t_object	*op;
{
    t_object *o1, *AddObject(), *GetObjectDescr();
    int has;		/* id only? */
    int with = 1;	/* object descriptor with data? */
    int id;		/* object id */

#ifdef OPTIMIZED
    memcpy(&has, *p, sizeof(int));
    *p += sizeof(int);
    assert(has == 0 || has == 1);
    if (has) {
	memcpy(&id, *p, sizeof(int));
	*p += sizeof(int);
	o1 = GetObjectDescr(id);
	assert(o1);
	mu_lock(&o1->o_mutex);
	o1->o_refcount++;
	o1->o_shared++;
	*op = *o1;
    } else {
#endif
	memcpy(&with, *p, sizeof(int));
	*p += sizeof(int);
	assert(with == 0 || with == 1);
	*p = ob_unmarshall(*p, op, d, with);
	/* initialize some fields */
	op->o_waitinglist = 0;
	mu_init(&op->o_mutex);
	op->o_refcount = 1;
#ifdef OPTIMIZED
	op->o_state = 0;
	mu_lock(&op->o_mutex);	/* lock it */
#endif
	if (o1 = AddObject(op)) {
		/* shared object already present on this cpu */
#ifdef OPTIMIZED
		assert(0);
#endif
		o1->o_refcount++;
		o1->o_shared++;
		/* can't use o_free, because it deletes id from the obj table */
		if (op->o_fields && with) {
			(*(td_objinfo(d)->obj_rec_free))(op->o_fields);
			m_free((char *) op->o_fields);
		}
		m_free(op->o_rtsdep);
		*op = *o1;
 	}
#ifdef OPTIMIZED
    }
#endif
}

/* Create shared objects. There are three type of shared objects in the msg:
 * 1) the id only; 2) the complete object; 3) the object descriptor.
 */
void create_obj(p, argv, pd)
    char *p;
    void **argv;
    prc_dscr *pd;
{
    int i;
    t_object *o;
    int nparams = td_nparams(pd->prc_func);
    register par_dscr *params = td_params(pd->prc_func);
    
    /* p points to first shared object */
    for (i = 0; i < nparams; i++, params++) {
	if (params->par_mode == SHARED) {
	    if (params->par_descr->td_type == ARRAY) {
		t_array *a = m_malloc(params->par_descr->td_size);
		register int j;
		tp_dscr *d = td_elemdscr(params->par_descr);

		memcpy(&(a->a_sz), p, sizeof(int));
		p += sizeof(int);
		a->a_offset = 0;
		memcpy(a->a_dims, p, td_ndim(params->par_descr) * 2 * sizeof(int));
		p += td_ndim(params->par_descr) * 2 * sizeof(int);
		for (j = 0; j < td_ndim(params->par_descr); j++) {
		    a->a_offset *= a->a_dims[j].a_nel;
		    a->a_offset += a->a_dims[j].a_lwb;
		}
		if (a->a_sz <= 0) {
		    a->a_data = 0;
		    continue;
		}

		o = m_malloc(a->a_sz * sizeof(t_object));
		a->a_data = (char *) o - a->a_offset * sizeof(t_object);

		for (j = a->a_sz; j > 0; j--, o++) {
		    get_obj(&p, d, o);
		}
		argv[i] = a;
	    }
	    else {
	    	o = m_malloc(sizeof(t_object));
	    	get_obj(&p, params->par_descr, o);
	    	argv[i] = o;
	    }
	}
    }
}

#ifdef OPTIMIZED

#define MAXNAME 20

static FILE 	*output;
static char 	out_name[MAXNAME];
static int	first_output = 1;
static t_objrts *obj_stat_list;

extern struct ObjectEntry *ObjectTable[];

static void dump_obj_table()
{
    int i;
    struct ObjectEntry *entry;

    fprintf(output, "======= object hash table =========\n");
    for (i = 0; i < MAXENTRIES; i++) {
	for (entry = ObjectTable[i]; entry != 0; entry = entry->obj_next) {
	    fprintf(output, "%3d: %3d nread %7d nwrite %5d nremote %5d\
 nreceived %5d nowner %5d\n",
		    entry->obj_id, entry->obj_descr.o_owner, entry->obj_descr.o_nread,
		    entry->obj_descr.o_nwrite, entry->obj_descr.o_nremote,
		    entry->obj_descr.o_nreceived, entry->obj_descr.o_nowner);
	}
    }
    fprintf(output, "==================================\n");
}


static void print_obj_state(o)
    t_object *o;
{
    int i;
    int id = o->o_id;
    t_obj_state *state = o->o_state;
    
    assert(output);
    
    fprintf(output, "%d: state for obj %d: replicated %d owner %d\n", 
	    this_cpu, id, o->o_replicated, o->o_owner);
    for (i = 0; i < ncpus; i++) {
	fprintf(output, 
		"  cpu %5d score %7d refcnt %3d replica %1d #access %7d\
 uncertain %7d\n",
		i, state[i].o_score, state[i].o_refcnt, state[i].o_replica,
		state[i].o_naccess, state[i].o_uncertainty);
    }
}


/* Given the current object state, compute an owner. If the object is already
 * owned by some cpu, move it to another cpu only if the other cpu performs
 * substantially more accesses (moving costs something too).
 */
static int decide_owner(old_owner, state)
    int old_owner;
    t_obj_state *state;
{
    int i;
    int a;
    int o;
    
    if (old_owner >= 0) {
	a = state[old_owner].o_naccess;
	o = old_owner;
    } else {
	a = -1 /* - THRESHHOLD */;
	o = -1;
    }

    for (i = 0; i < ncpus; i++) {
	if(state[i].o_naccess > (a /* + THRESHHOLD */)) {
	    a = state[i].o_naccess;
	    o = i;
	}
    }
    assert(o >= 0 && o < ncpus);
    
    return(o);
}


static int compare_cost(nbroadcast, nrpc)
    int nbroadcast;
    int nrpc;
{
    int cost1, cost2;

    cost2 = nbroadcast * COST_BROADCAST(ncpus);
    cost1 = nrpc * COST_RPC;
    
    return(cost2 <= cost1);
}



static int sum_refcnt(state)
    t_obj_state *state;
{
    int refcnt = 0;
    int i;

    for (i = 0; i < ncpus; i++) refcnt += state[i].o_refcnt;

    return(refcnt);
}


static void master_all_on_zero(o)
    t_object *o;
{
    int i = 0;
    
    o->o_owner = 0;
    o->o_replicated = 0;
    if(this_cpu != 0 && o->o_state[this_cpu].o_replica) {
	(*(td_objinfo(o->o_dscr)->obj_rec_free))(o->o_fields);
    }
    for (i = 1; i < ncpus; i++) {
	o->o_state[i].o_replica = 0;
    }
}


static void replicate_all_or_master(o)
    t_object *o;
{
    int nrpc = 0;
    int nbroadcast = 0;
    int potential_owner;
    int old_owner = o->o_owner;
    int below_threshhold = 1;
    int replicate;
    int i;
    int nrefs = sum_refcnt(o->o_state);

    if (nrefs > MAX_RPC_THREADS) replicate = 1;
    else {
    	potential_owner = decide_owner(old_owner, o->o_state);

    	for (i = 0; i < ncpus; i++) {
		nbroadcast += (o->o_state[i].o_naccess - o->o_state[i].o_score) / 2;
		if (i != potential_owner) nrpc += o->o_state[i].o_naccess;
		below_threshhold = below_threshhold && o->o_state[i].o_naccess
	    		< THRESHHOLD;
    	}
    
    	replicate = compare_cost(nbroadcast, nrpc);
    }
    
    if (below_threshhold || replicate) { /* replicate? */
	if (!o->o_replicated) {
	    /* Object changes from not replicated to replicated. */
	    o->o_owner = -1;
	    o->o_replicated = 1;
	    if (this_cpu == old_owner) {
		for (i = 0; i < ncpus; i++) {
		    if (i != this_cpu && o->o_state[i].o_refcnt > 0) {
			give_copy(o, i);
			o->o_state[i].o_replica = 1;
		    }
		}
		touched(o);
	    } else {
		sema_down(&o->o_semdata); /* wait until we get data for obj */
	    }
	}
	assert(o->o_owner == -1);
    } else {		/* no, make one cpu owner */
	o->o_replicated = 0;
	o->o_owner = potential_owner;
	o->o_refcount = nrefs; /* update refcnts */
	
	if (old_owner >= 0 && old_owner != o->o_owner) {
	    /* Object changes from owner. */
	    if(this_cpu == old_owner) { /* this cpu old owner? */
	        give_copy(o, o->o_owner);
	    } else if (this_cpu == o->o_owner) {  /* this cpu new owner? */
		sema_down(&o->o_semdata); /* wait until we get data for obj */
	    }
	}

	if (this_cpu != o->o_owner) {
	    /* The data is no longer needed; free it. */
	    (*(td_objinfo(o->o_dscr)->obj_rec_free))(o->o_fields);
	}
	
	for (i = 0; i < ncpus; i++) {
	    if(i != o->o_owner) o->o_state[i].o_replica = 0;
	}

	if (old_owner != o->o_owner && (old_owner == -1 || old_owner == this_cpu))
	{
	    /* Object changed state; let all threads retry their operation. */
	    touched(o);
	}
	
    }
}


static void make_decision(o)
    t_object *o;
{
    if (o->o_strategy_set) {
	register int i;

	if (o->o_replicated) return;
	o->o_refcount = sum_refcnt(o->o_state);
	if (this_cpu != o->o_owner && o->o_fields) {
	    /* The data is no longer needed; free it. */
	    (*(td_objinfo(o->o_dscr)->obj_rec_free))(o->o_fields);
	    o->o_fields = 0;
	}

	for (i = 0; i < ncpus; i++) {
	    if(i != o->o_owner) o->o_state[i].o_replica = 0;
	}

	if (o->o_strategy_set == 1)
	{
	    /* Object changed state; let all threads retry their operation. */
	    touched(o);
	}
	o->o_strategy_set++;
	return;
    }

    switch(replication_policy) {
    case MASTER_ALL_ON_ZERO:
	master_all_on_zero(o);
	break;
    case REPLICATE_ALL:
	/* default strategy: nothing */
	o->o_replicated = 1;
	o->o_owner = -1;
	break;
    case REPLICATE_ALL_OR_MASTER:
	replicate_all_or_master(o);
	break;
    }
    if (rts_verbose > 1) 
	fprintf(output, "decision: %3d replicated %4d owner %4d refcnt %4d\n",
		o->o_id, o->o_replicated, o->o_owner, o->o_refcount);
}


static void table_make_decision()
{
    int i;
    struct ObjectEntry *entry;
    register t_object *o;

    for (i = 0; i < MAXENTRIES; i++) {
	for (entry = ObjectTable[i]; entry != 0; entry = entry->obj_next) {
	    o = &(entry->obj_descr);
	    mu_lock(&o->o_mutex);
	    make_decision(o);
	    mu_unlock(&o->o_mutex);
	}
    }
}

static void
process_obj(statep, sh_argi, cpu, nfork)
    char	**statep;
    sh_args	*sh_argi;
    int		cpu;
    int		nfork;
{
    int		id;
    t_object *o, *GetObjectDescr();	/* object */
    t_obj_state state;			/* state for cpu, if refcnt == 0 */
    char	*p = *statep;

    memcpy(&id, *statep, sizeof(int)); /* get object id */
    *statep += sizeof(int);
    o = GetObjectDescr(id);	/* set pointer to object descr */
    if (o == 0) {		/* we do not have the object */
	*statep += sizeof(t_obj_state) * ncpus;
    } else {			/* we got the object */
	if(this_cpu != cpu) mu_lock(&o->o_mutex);	/* lock it */
	if(o->o_state == 0) {	/* we just got it */
	    assert(this_cpu == cpu);
	    o->o_state = (t_obj_state *) m_malloc(sizeof(t_obj_state)
						 * ncpus);
	    assert(o->o_state);
	    memcpy(o->o_state, *statep, sizeof(t_obj_state) * ncpus);
	    state = o->o_state[this_cpu];
	} else {
	    memcpy(&state, *statep + sizeof(t_obj_state) * cpu,
		  sizeof(t_obj_state));
	}
	*statep += sizeof(t_obj_state) * ncpus;

	/* update obj_state for cpu on which the process is forked */
	if(o->o_state[cpu].o_refcnt > 0) {
	    o->o_state[cpu].o_score += (int)(sh_argi->arg_score);
	    o->o_state[cpu].o_naccess += (int)(sh_argi->arg_naccess);
	} else {
	    o->o_state[cpu].o_score = (int)(sh_argi->arg_score);
	    o->o_state[cpu].o_naccess = (int)(sh_argi->arg_naccess);
	}
	o->o_state[cpu].o_refcnt++;
	if (nfork > ncpus - 1) {
	    if (rts_verbose > 1) print_obj_state(o);
	    make_decision(o);
	}
	mu_unlock(&o->o_mutex);	/* unlock it */
    }
}

/* Walk through the arguments of the forked process on cpu. For each shared
 * object update the object state and decide what to do with it. There are
 * two decisions possible: 1) it is replicated on all processors; 2) it
 * is stored on a single processor. Once it is stored on a single processor
 * it stays there for its life. We start taking decisions after a process has
 * been forked on each cpu.
 */
void process_obj_state(nfork, pd, cnt, statep, cpu)
    int nfork;
    prc_dscr *pd;
    int cnt;
    char *statep;			/* pointer to marshalled obj state */
    int cpu;				/* cpu on which is forked */
{
    int i;
    int j;
    int nparams = td_nparams(pd->prc_func);
    register par_dscr *params = td_params(pd->prc_func);
 
    /* Set output FILE. */
    if (rts_verbose > 1) {
	if (first_output) {
	    sprintf(out_name, "obj_state.%d", this_cpu);
	    output = fopen(out_name, "w");
	    first_output = 0;
	} else output = fopen(out_name, "a");
	fprintf(output, "============ process_score ===========\n");
    }

    /* Update state for each shared object and make decision. */
    for (i = 0; i < nparams; i++, params++) {
	if (params->par_mode == SHARED) {
	    if (params->par_descr->td_type == ARRAY) {
		int sz;
		memcpy(&sz, statep, sizeof(int));
		statep += sizeof(int);
		cnt -= sizeof(int);
		for (j = sz; j > 0; j--) {
		    process_obj(&statep, &pd->prc_shargs[i], cpu, nfork);
		    cnt -= ncpus * sizeof(t_obj_state) + sizeof(int);
		}
	    }
	    else {
	        process_obj(&statep, &pd->prc_shargs[i], cpu, nfork);
		cnt -= ncpus * sizeof(t_obj_state) + sizeof(int);
	    }
	}
	
    }

    assert(cnt == 0);
    if (nfork == ncpus - 1) {
	/* A process has been forked on each cpu; determine locations
	 * for each object in the object table.
	 */
	if (rts_verbose > 1) dump_obj_table();
	table_make_decision();
    }
    if (rts_verbose > 1) {
	fprintf(output, "======================================\n");
	fclose(output);
    }
}


void stat_keep_descr(o)
    t_object *o;
{
    if (obj_stat_list == 0) {	/* first? */
	obj_stat_list = o->o_rtsdep;
	o->o_next = 0;
    } else {
	o->o_next = obj_stat_list;
	obj_stat_list = o->o_rtsdep;
    }
    o->o_rtsdep = 0;
}


void print_obj_stat()
{
    t_objrts *o;
    
    
    if (rts_verbose > 0) {
	if (first_output) {
	    sprintf(out_name, "obj_state.%d", this_cpu);
	    output = fopen(out_name, "w");
	    first_output = 0;
	} else output = fopen(out_name, "a");
	
	dump_obj_table();
	
	for (o = obj_stat_list; o != 0; o = o->oo_next) {
	    fprintf(output, "%3d: %3d nread %7d nwrite %5d nremote %5d\
 nreceived %5d nowner %5d\n", o->oo_id, o->oo_owner, o->oo_nread, o->oo_nwrite,
		    o->oo_nremote, o->oo_nreceived, o->oo_nowner);
	}
	fclose(output);
    }
}


static void print_bin(str, n, bin)
    int n;
    char *str;
    int *bin;
{
    int i;

    fprintf(output, "%s : ", str);
    for (i = 0; i < n; i++) {
	fprintf(output, "%5d", bin[i]);
    }
    fprintf(output, "\n");
}


void print_bin_stat(n, rpc_getreq_bin, rpc_putrep_bin, rpc_putreq_bin,
		    rpc_getrep_bin, grp_rcv_bin, grp_snd_bin)
    int n;
    int *rpc_getreq_bin;
    int *rpc_putrep_bin;
    int *rpc_putreq_bin;
    int *rpc_getrep_bin;
{
    int i;

    if (rts_verbose > 0) {
	if (first_output) {
	    sprintf(out_name, "obj_state.%d", this_cpu);
	    output = fopen(out_name, "w");
	    first_output = 0;
	} else output = fopen(out_name, "a");
	
	fprintf(output, ">= size: ");
	for (i = 0; i < n; i++) {
	    fprintf(output, "%5d", (1 << i) - 1);
	}
	fprintf(output, "\n");
	print_bin("getreq", n, rpc_getreq_bin);
	print_bin("putrep", n, rpc_putrep_bin);
	print_bin("putreq", n, rpc_putreq_bin);
	print_bin("getrep", n, rpc_getrep_bin);
	print_bin("grprcv", n, grp_rcv_bin);
	print_bin("grpsnd", n, grp_snd_bin);

	fclose(output);
    }
}
#endif


void print_op_stat()
{
    if (rts_verbose > 0) {
#ifdef OPTIMIZED
	if (first_output) {
	    sprintf(out_name, "obj_state.%d", this_cpu);
	    output = fopen(out_name, "w");
	    first_output = 0;
	} else output = fopen(out_name, "a");
	
	fprintf(output, "local ops cpu %d:\n\ttotal %d\n", this_cpu,
		opstat.oo_nlocal);
	fprintf(output, "grp ops cpu %d:\n", this_cpu);
	fprintf(output, "\tread guards %d succeeded %d failed %d\n",
		opstat.oo_nread_trial, opstat.oo_nread, opstat.oo_nread_trial -
		opstat.oo_nread);
	fprintf(output, "\tbcast operations %d succeeded %d failed %d\n",
		opstat.oo_nwrite_trial, opstat.oo_nwrite, opstat.oo_nwrite_trial -
		opstat.oo_nwrite);
	fprintf(output, "\tblocked: %d\n", opstat.oo_nblock);
	fprintf(output, "rpc ops cpu %d:\n\tlocal %d\n\tremote %d\n",
		this_cpu, opstat.r_nlocal, opstat.r_nremote);
#ifdef PROFILING
	prof_dump(output);
#endif
	fclose(output);
#else
    	fprintf(file, "local ops cpu %d: %d\n", this_cpu, opstat.oo_nlocal);
    	fprintf(file, "grp ops cpu %d: shared read %d shared write %d\n", 
	    	this_cpu, opstat.oo_nread, opstat.oo_nwrite);
    	fprintf(file, "rpc ops cpu %d: local %d remote %d\n", this_cpu, opstat.r_nlocal,
	    	opstat.r_nremote);
    	fflush(file);
#endif
    }
}
