/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#include "interface.h"
#include "obj_tab.h"
#include "fragment.h"
#include "manager.h"
#include "rts_globals.h"
#include "rts_object.h"
#include "rts_trace.h"
#include "continuation.h"

/* Marshalling of shared objects; three cases:
 * 1) Send id only
 * 2) Send reference (including manager location)
 * 3) Send data (for replicated object)
 */


/***** case 1) *****/

/********************************************************************/

static int
do_shared_nbytes(void *arg, tp_dscr *descr, int cpu)
{
#ifdef DATA_PARALLEL
    if (descr->td_flags & PARTITIONED) {
	return sizeof(int);
    }
#endif
    return sizeof(shared_id_t);
}

static char *
do_shared_marshall(char *p, void *arg, tp_dscr *descr, int cpu)
{
    fragment_p f = (fragment_p)arg;
    shared_id_t sid;

#ifdef DATA_PARALLEL
    if (descr->td_flags & PARTITIONED) {
	memcpy(p, &(((instance_p) arg)->id), sizeof(int));
	return p + sizeof(int);
    }
#endif
    sid.si_oid    = f->fr_oid;
    sid.si_object = f;
    
    /* Hack for dynamic arrays; __Score() is called before
     * the user has had a chance to initialize the array.
     * Call __Score() again with the initialized object.
     */
    if (f_get_status( f) == f_unshared) {
	__Score( f, descr, (double) 0, (double) 0, (double) 481);
    }
    (void)memcpy(p, &sid, sizeof(shared_id_t));
    return p + sizeof(shared_id_t);
}

static char *
do_shared_unmarshall(char *p, void *arg, tp_dscr *descr)
{
#ifdef DATA_PARALLEL
    if (descr->td_flags & PARTITIONED) {
	int i;
	memcpy(&i, p, sizeof(int));
	*(instance_p *) arg = get_instance(i);
	return p + sizeof(int);
    }
#endif
    (void)memcpy((char *)arg, p, sizeof(shared_id_t));
    return p + sizeof(shared_id_t);
}

/********************************************************************/

int
o_shared_nbytes(void *arg, tp_dscr *descr, int cpu)
{
    if ( descr->td_type == ARRAY) {
	t_array *a = arg;
	int nbytes;

	nbytes = sizeof(a->a_sz);
	nbytes += td_ndim(descr) * 2 * sizeof(a->a_dims[0].a_lwb);

	descr = td_elemdscr(descr);
	nbytes += a->a_sz *
	  do_shared_nbytes((char *)a->a_data + a->a_offset*descr->td_size,
			   descr, cpu);
	return nbytes;
    }
    else {
	return do_shared_nbytes(arg, descr, cpu);
    }
}


char *
o_shared_marshall(char *p, void *arg, tp_dscr *descr, int cpu)
{
    if ( descr->td_type == ARRAY) {
	t_array *a = arg;
	char *data;
	int i;

	put_tp(a->a_sz, p);
	for (i = 0; i < td_ndim(descr); i++) {
	    put_tp(a->a_dims[i].a_lwb, p);
	    put_tp(a->a_dims[i].a_nel, p);
	}

	descr = td_elemdscr(descr);
	data = (char *) a->a_data + a->a_offset * descr->td_size;
	for ( i=0; i < a->a_sz; i++) {
	    p = do_shared_marshall( p, (void *) data, descr, cpu);
	    data += descr->td_size;
	}
	return( p);
    } else {
	return( do_shared_marshall( p, arg, descr, cpu));
    }
}


char *
o_shared_unmarshall(char *p, void **arg, tp_dscr *descr)
{
    if ( descr->td_type == ARRAY) {
	t_array *a;
	char *ptr;
	int i;
	
	*arg = a = m_malloc( sizeof( t_array));
	
	get_tp(a->a_sz, p);
	a->a_offset = 0;
	for (i = 0; i < td_ndim(descr); i++) {
	    get_tp(a->a_dims[i].a_lwb, p);
	    get_tp(a->a_dims[i].a_nel, p);
	    a->a_offset *= a->a_dims[i].a_nel;
	    a->a_offset += a->a_dims[i].a_lwb;
	}
	
	ptr = (char *)m_malloc(a->a_sz * sizeof(shared_id_t));
	a->a_data = ptr - a->a_offset * sizeof(shared_id_t);
	
	descr = td_elemdscr(descr);
	for ( i=0; i < a->a_sz; i++) {
	    p = do_shared_unmarshall(p, (void *)ptr, descr);
	    ptr += sizeof(shared_id_t);
	}
	return( p);
    }
    else {
	*arg = m_malloc(sizeof(shared_id_t));
	return do_shared_unmarshall(p, *arg, descr);
    }
}

/***** case 2) *****/
/***** case 3) *****/

int
rts_prepare_object_marshall(fragment_p obj)
{
    f_status_t s = f_get_status(obj) & ~RO_MANAGER;
    int send_data_bit, marshall_flags;

    switch (f_get_status(obj)) {
      case f_manager:
      case f_replicated:
	s = f_replicated;
	break;
      case f_in_transit:		/* take old status (f_owner) */
      case f_owner:
      case f_remote:
	s = f_remote;
	break;
      default:
	fprintf(stderr, "rts_prepare_object_marshall: unknown status 0x%x\n",
		f_get_status(obj));
	abort();
    }
    
    /* Note that object is known at other CPUs so take_decision() won't
     * mess up if reference count drops to 1
     */
    obj->fr_flags &= ~MAN_LOCAL_ONLY;
    
    /* owner change; upgrade status
     */
    if (obj->fr_manager && obj->fr_owner != rts_my_pid) {
	s |= RO_MANAGER;
	obj->fr_flags &= ~RO_MANAGER;  /* just in case BC is too late */
    }
    
    send_data_bit  = s & (RO_REPLICATED|RO_MANAGER) ? 1 : 0;
    marshall_flags = (s << 1) | send_data_bit;

    return marshall_flags;
}

int
rtspart_nbytes(t_object *op, tp_dscr *d, int flags)
{
    fragment_p obj = (fragment_p)op;
    f_status_t s   = (flags >> 1) & RO_MASK;
    int sz;

    /* 1. size of RTS part of object */
    sz = 
#ifdef TRACING
      sizeof(short) +                     /* home location of object */
      sizeof(fragment_p) +                /* object ptr at home location */
#endif
      sizeof(int) +
      sizeof(int) +
      sizeof(int) + strlen(obj->fr_name) + 1;

    /* 2. size of manager data */
    if (s & RO_MANAGER) {
	sz += man_nbytes(obj);
    }
    return sz;
}

char *
rtspart_marshall(char *p, t_object *op, tp_dscr *d, int flags)
{
    fragment_p obj = (fragment_p)op;
    f_status_t s   = (flags >> 1) & RO_MASK;
    int len;

#ifdef TRACING
    (void)memmove(p, &obj->fr_home, sizeof(short));
    p += sizeof(short);
    (void)memmove(p, &obj->fr_home_obj, sizeof(fragment_p));
    p += sizeof(fragment_p);
#endif
    (void)memmove(p, &(obj->fr_total_refs), sizeof(int));
    p += sizeof(int);

    (void)memmove(p, &(obj->fr_owner), sizeof(int));
    p += sizeof(int);
    
    len = strlen(obj->fr_name) + 1;
    (void)memmove(p, &len, sizeof(int));
    p += sizeof(int);
    (void)memmove(p, obj->fr_name, len);
    p += len;
    
    if (s & RO_MANAGER) {
	p = man_marshall(p, obj);
    }

    return p;
}

char *
rtspart_unmarshall(char *p, t_object *op, tp_dscr *d, int flags)
{
    fragment_p obj = (fragment_p)op;
    f_status_t s   = (flags >> 1) & RO_MASK;
    int len;

    if (s == 0) {  /* object passed by value; allocate RTS info */
	f_init(obj, d, "by-value copy");
#ifdef TRACING
        p += sizeof(short);
        p += sizeof(fragment_p);
#endif
        p += sizeof( int);   /* fr_total_refs */
        p += sizeof( int);   /* fr_owner */
        (void)memmove(&len, p, sizeof(int));
        p += sizeof( int);
        p += len;            /* fr_name */

	return p;
    }

#ifdef TRACING
    f_trc_create( op, "unmarshalled copy?");

    (void)memmove(&obj->fr_home, p, sizeof(short));
    p += sizeof(short);
    (void)memmove(&obj->fr_home_obj, p, sizeof(fragment_p));
    p += sizeof(fragment_p);
#endif

    obj->fr_flags &= ~RO_MASK;
    obj->fr_flags |= s;
    
    if (obj->fr_owner == RO_NO_OWNER) {
	(void)memmove(&(obj->fr_total_refs), p, sizeof(int));
    }
    p += sizeof( int);
    
    (void)memmove(&(obj->fr_owner), p, sizeof(int));
    p += sizeof( int);
    
    (void)memmove(&len, p, sizeof(int));
    p += sizeof( int);
    obj->fr_name = (char *)m_realloc(obj->fr_name, len);
    (void)memmove(obj->fr_name, p, len);
    p += len;
    
#ifdef RTS_VERBOSE
    printf("%d) unmarshalled object %s\n", rts_my_pid, obj->fr_name);
#endif

    if (s & RO_MANAGER) {
	assert( obj->fr_owner == rts_my_pid);
	obj->fr_manager = m_malloc(sizeof(manager_t));
	p = man_unmarshall(p, obj);
    }
    
    return p;
}

void
__Score(void *data, tp_dscr *d, double score, double naccess, double uncertainty)
{
    switch(d->td_type) {
    case OBJECT: {
	fragment_p f = data;

	assert( f_get_status(f) == f_unshared);

	f->fr_flags &= ~RO_MASK;
	f->fr_flags |= f_owner;
	f->fr_info.access_sum = naccess;
	f->fr_owner = rts_my_pid;
	f->fr_manager = m_malloc(sizeof(manager_t));
#ifdef RTS_VERBOSE
	printf( "score for obj %s: score = %g, naccess = %g\n", f->fr_name,
	       score, naccess);
#endif
	man_init( f->fr_manager, (float) (score+naccess)/2.0, (float) (naccess-score)/2.0);
	f->fr_flags |= MAN_LOCAL_ONLY;		/* part of man_init()? */
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
	    /* Compiler does not analyze arrays of shared objects proper.
	     * Passing zero scores to let the rts take over, however, does
	     * not work in the case of a single master communicating to
	     * multiple slaves through individual objects. The slaves only
	     * receive a single object and apply the compiler score, so wes
	     * have to do the same for the master.
	     * Hope that this does not interfere with the case where slaves
	     * receive an array of shared objects, because then the slaves
	     * neglect the compiler scores, but the master does not.
	     */
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
}

void
__erocS(void *data, tp_dscr *d, double score, double naccess, double uncertainty)
{
    switch(d->td_type) {
    case OBJECT: {
	man_delete(data, (float) (score+naccess)/2.0,
		   (float) (naccess-score)/2.0, 1);
	break;
	}
    case ARRAY: {
    	t_array *a = data;
	char *p;
	
	a = data;
	d = td_elemdscr(d);
	p = &((char *)(a->a_data))[a->a_offset * d->td_size];
	man_delete((fragment_p)p, (float) (score+naccess)/2.0,
		   (float) (naccess-score)/2.0, a->a_sz);
	break;
	}
    case RECORD:
    case GRAPH:
	/* ??? */
	break;
    }
}


/* Tells which strategy to use for obj. */
void
m_strategy(t_object *obj, int replicated, int owner)
{
    int cpu = owner + rts_base_pid;

    man_strategy((fragment_p) obj, replicated, cpu);
}

int
o_free(t_object *o)
{
    if (o->o_rtsdep) {
	o_kill_rtsdep(o);
	return 1;
    }
    return 0;
}

void
m_print(FILE *f, char *s, int len)
{
    fwrite(s, 1, len, f);
}

int
m_scan(FILE *f, char *s, void *p)
{
    return fscanf(f, s, p);
}

void
m_objdescr_reg(tp_dscr *obj_dscr, int n_operations, char *name)
{
    int i;
    struct {short type; short indx; char name[80];} info;
    
    if (rts_my_pid > 0) {	/* one will do nicely */
	return;
    }
    
    trc_event( objtype_descr, map_info(td_registration(obj_dscr), name));
    for (i = 0; i < n_operations; i++) {
	info.type = td_registration(obj_dscr);
	info.indx = i;
	strncpy(info.name, td_operations(obj_dscr)[i].op_trc_name, 80);
	trc_event(op_descr, (void *)&info);
    }
}
