#include <stdarg.h>
#include "rts_internals.h"
#include "obj_tab.h"
#include "fragment.h"
#include "manager.h"
#include "orca_types.h"
#include "marshall.h"
#include "rts_object.h"
#include "rts_trace.h"
#include "interface.h"
#include "continuation.h"


/* Marshalling of shared objects; three cases:
 * 1) Send id only
 * 2) Send reference (including manager location)
 * 3) Send data (for replicated object)
 */


/***** case 1) *****/

/* Two macros from dataman/marshall.c */

#define get_tp(dst, p)  \
        { register int __i; register char *__q = (char *) &(dst); \
          for (__i = sizeof(dst); __i > 0; __i--) { \
            *__q++ = *p++; \
          } \
        }

#define put_tp(src, p)  \
        { register int __i; register char *__q = (char *) &(src); \
          for (__i = sizeof(src); __i > 0; __i--) { \
            *p++ = *__q++; \
          } \
        }


static int
do_shared_nbytes(void *arg, tp_dscr *descr, int cpu)
{
    return sizeof(oid_t);
}

static char *
do_shared_marshall(char *p, void *arg, tp_dscr *descr, int cpu)
{
    fragment_p f = (fragment_p) arg;
    
    /* Hack for dynamic arrays; __Score() is called before
     * the user has had a chance to initialize the array.
     * Call __Score() again with the initialized object.
     */
    if (f_get_status( f) == f_unshared) {
	__Score( f, descr, (double) 0, (double) 0, (double) 481);
    }
    oid_marshall( &(f->fr_oid), p);
    return p + sizeof(oid_t);
}

static char *
do_shared_unmarshall(char *p, void *arg, tp_dscr *descr)
{
    oid_unmarshall( (oid_p) arg, p);
    return p + sizeof(oid_t);
}


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
	return( nbytes);
    }
    else {
	return( do_shared_nbytes( arg, descr, cpu));
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


char *o_shared_unmarshall(char *p, void **arg, tp_dscr *descr)
{
	if ( descr->td_type == ARRAY) {
		t_array *a;
		char *ptr;
		int i;

		*arg = a = sys_malloc( sizeof( t_array));

		get_tp(a->a_sz, p);
		a->a_offset = 0;
		for (i = 0; i < td_ndim(descr); i++) {
		        get_tp(a->a_dims[i].a_lwb, p);
        		get_tp(a->a_dims[i].a_nel, p);
        		a->a_offset *= a->a_dims[i].a_nel;
        		a->a_offset += a->a_dims[i].a_lwb;
  		}

		ptr = (char *) sys_malloc(a->a_sz * sizeof( oid_t));
		a->a_data = ptr - a->a_offset * sizeof( oid_t);

		descr = td_elemdscr( descr);
		for ( i=0; i < a->a_sz; i++) {
			p = do_shared_unmarshall( p, (void *)ptr, descr);
			ptr += sizeof(oid_t);
		}
		return( p);
	}
	else {
		*arg = sys_malloc( sizeof( oid_t));
		return( do_shared_unmarshall( p, *arg, descr));
	}
}

/***** case 2) *****/
/***** case 3) *****/

int
nbytes_object( fragment_p obj)
{
	f_status_t s = f_get_status( obj) & ~RO_MANAGER;
	int sz = 0;

	/* owner change; upgrade status */
	if ( obj->fr_manager && obj->fr_owner != sys_my_pid)
		s |= RO_MANAGER;

	sz += sizeof( f_status_t);
	sz += sizeof( int);
	sz += sizeof( int);
	sz += sizeof(int) + strlen( obj->fr_name);
	if ( s & (RO_REPLICATED|RO_MANAGER))
		sz += r_nbytes( f_get_fields(obj), td_objrec(f_get_type(obj)));
	if ( s & RO_MANAGER)
		sz += man_nbytes( obj);

	return( sz);
}


char *
marshall_object( char *buf, fragment_p obj)
{
	int len;
	f_status_t s;

	switch ( f_get_status( obj)) {
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
		fprintf( stderr, "marshall_object: unknow status 0x%x\n",
			 f_get_status( obj));
		abort();
	}

	/* Note that object is known at other CPUs so take_decision() won't
	 * mess up if reference count drops to 1
	 */
	obj->fr_flags &= ~MAN_LOCAL_ONLY;

	/* owner change; upgrade status */
	if ( obj->fr_manager && obj->fr_owner != sys_my_pid) {
		s |= RO_MANAGER;
		obj->fr_flags &= ~RO_MANAGER;  /* just in case BC is too late */
	}

	(void)memmove(buf, &s, sizeof(f_status_t));
	buf += sizeof( f_status_t);

	(void)memmove(buf, &(obj->fr_total_refs), sizeof(int));
	buf += sizeof( int);

	(void)memmove(buf, &(obj->fr_owner), sizeof(int));
	buf += sizeof( int);

	len = strlen( obj->fr_name);
	(void)memmove(buf, &len, sizeof(int));
	buf += sizeof(int);
	(void)memmove(buf, obj->fr_name, len);
	buf += len;

	if ( s & (RO_REPLICATED|RO_MANAGER)) {
		buf = r_marshall(buf, f_get_fields(obj), td_objrec(f_get_type(obj)));
	}
	if ( s & RO_MANAGER)
		buf = man_marshall(buf, obj);

	return( buf);
}


char *
unmarshall_object( char *buf, fragment_p obj)
{
	int len;
	f_status_t s;

	(void)memmove( &s, buf, sizeof(f_status_t));
	buf += sizeof( f_status_t);
	obj->fr_flags &= ~RO_MASK;
	obj->fr_flags |= s;

	if ( obj->fr_owner == RO_NO_OWNER)
		(void)memmove(&(obj->fr_total_refs), buf, sizeof(int));
	buf += sizeof( int);

	(void)memmove(&(obj->fr_owner), buf, sizeof(int));
	buf += sizeof( int);

	(void)memmove(&len, buf, sizeof(int));
	buf += sizeof( int);
	obj->fr_name = (char *)sys_realloc(obj->fr_name, len);
	(void)memmove(obj->fr_name, buf, len);
	buf += len;

	/* At OrcaMain and with arrays of shared objects two instances of
	 * the "same" structure t_object will be created. To avoid
	 * inconsistencies, always pre-allocate o_fields eventhough the data
	 * may never be stored at this site.
	 */
	if ( f_get_fields(obj) == 0)			/* avoid stale copy! */
		f_get_fields(obj) = sys_malloc( td_objrec(f_get_type(obj))->td_size);

	if ( s & (RO_REPLICATED|RO_MANAGER)) {
		if ( obj->fr_flags & MAN_VALID_FIELD)	/* avoid space leak */
			r_free( f_get_fields(obj), td_objrec(f_get_type( obj)));
		else
			obj->fr_flags |= MAN_VALID_FIELD;

		buf = r_unmarshall(buf, f_get_fields(obj), td_objrec(f_get_type(obj)));
	}

	if ( s & RO_MANAGER) {
		assert( obj->fr_owner == sys_my_pid);

		obj->fr_manager = (manager_p) sys_malloc( sizeof( manager_t));
		buf = man_unmarshall( buf, obj);
	}

	return( buf);
}



int o_rts_nbytes( t_object *op, tp_dscr *d, int flags)
{
	return( 0);
}


char *o_rts_marshall( char *p, t_object *op, tp_dscr *d, int flags)
{
	return( p);
}


char *o_rts_unmarshall( char *p, t_object *op, tp_dscr *d, int flags)
{
        f_init( (fragment_p) op, d, "unmarshalled copy?");
	f_trc_create( op, "unmarshalled copy?");
	return( p);
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
	f->fr_owner = sys_my_pid;
	f->fr_manager = (manager_p) sys_malloc( sizeof(manager_t));
/* printf( "score for obj %s: score = %g, naccess = %g\n", oid_ascii( &f->fr_oid), score, naccess); */
	man_init( f->fr_manager, (score+naccess)/2, (naccess-score)/2);
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
	fragment_p f = data;

	/* Note that it is possible with arrays of (shared) objects that
	 * Score() is never called, so objects do not have to be removed
	 * from the object table.
	 */
	f_lock(f);
	if ( f_get_status(f) != f_unshared) {
		man_delete( f, (score+naccess)/2, (naccess-score)/2);
		f->o_rtsdep = 0;		/* fool o_free() */
	} else {
		f_unlock(f);
	}
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
	    __erocS(p, d, score/a->a_sz, naccess/a->a_sz, uncertainty/a->a_sz);
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


/* Tells which strategy to use for obj. */
void
m_strategy(t_object *obj, int replicated, int owner)
{
	extern int rts_base_pid;

	man_strategy( (fragment_p) obj, replicated, owner+rts_base_pid);
}


void o_free(void *op, tp_dscr *d)
{
	register fragment_p obj = op;

	if ( obj->o_rtsdep) {
		o_deallocate( (t_object *) op, d);
		o_kill_rtsdep( (t_object *) op);
	}
}

void m_print(FILE *f, char *s, int len)
{
  fwrite(s, 1, len, f);
}

int m_scan(FILE *f, char *s, void *p)
{
  return fscanf(f, s, p);
}


void m_objdescr_reg(tp_dscr *obj_dscr, int n_operations, char *name)
{
	int i;
	struct {short type; short indx; char name[80];} info;

	if ( sys_my_pid > 1)	/* one will do nicely */
		return;

	trc_event( objtype_descr, map_info( td_registration(obj_dscr), name));
	for ( i=0; i < n_operations; i++) {
		info.type = td_registration(obj_dscr);
		info.indx = i+2;
		strncpy( info.name, td_operations(obj_dscr)[i].op_trc_name, 80);
		trc_event( op_descr, (void *)&info);
	}
}
