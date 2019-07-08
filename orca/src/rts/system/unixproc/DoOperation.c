/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: DoOperation.c,v 1.16 1998/06/11 12:00:59 ceriel Exp $ */

#include <interface.h>
#include "unixproc.h"
#ifdef TEST_MARSHALL
#include "test_marshall.h"
#endif

void
DoOperation(register t_object *o, int *op_flags, tp_dscr *d, int opindex, int att, void **argtab)
{
  register struct op_descr *op;
  int written = 0;
  void **argp = argtab;
  
  if (*op_flags & BLOCKING) return;

  op = td_operations(d);
  op = &op[opindex];

#ifdef TEST_MARSHALL
  marshall_and_unmarshall_args(op, argtab, &argp);
#endif

  for (;;) {
  	if (! (*op_flags & NESTED) && o_isshared(o)) {
  		pthread_mutex_lock(&o->o_mutex);
		/* immediate unlock if the object is not shared
		   after all.
		*/
		if (! o_isshared(o)) {
  			pthread_mutex_unlock(&o->o_mutex);
		}
  	}
  	if (op->op_read_alts) {
		if ((*op->op_read_alts)(o, argp)) {
			*op_flags |= BLOCKING;
		    	if (op->op_write_alts) {
				if (! (*op->op_write_alts)(o, argp)) {
					*op_flags &= ~BLOCKING;
					if (o->o_procsblocking) {
						written = 1;
					}
				}
			}
  		}
  	}
  	else {
		if (! (*op->op_write_alts)(o, argp)) {
			if (o->o_procsblocking) {
				written = 1;
			}
		}
		else	*op_flags |= BLOCKING;
  		
  	}
  	if (! (*op_flags & NESTED) && o_isshared(o)) {
  		if (*op_flags & BLOCKING) {
  			*op_flags &= ~BLOCKING;
			o->o_procsblocking++;
  			pthread_cond_wait(&o->o_cond, &o->o_mutex);
			o->o_procsblocking--;
  			pthread_mutex_unlock(&o->o_mutex);
  			continue;
  		}
  		pthread_mutex_unlock(&o->o_mutex);
  	}

	if (! (*op_flags & NESTED) && written && o_isshared(o)) {
  		pthread_cond_broadcast(&o->o_cond);
	}
  	break;
  }
  if (*op_flags & BLOCKING) {
  	assert((*op_flags & NESTED) || ! o_isshared(o));
  	if (! (*op_flags & NESTED)) m_trap(LOCAL_DEADLOCK, (char *) 0, 0);
  }
#ifdef TEST_MARSHALL
  marshall_and_unmarshall_return(op, argp, argtab);
#endif
}

t_boolean
o_start_read(register t_object *o)
{
#ifdef TEST_MARSHALL
  return 0;
#else
  if (o_isshared(o)) {
	pthread_mutex_lock(&o->o_mutex);
	if (! o_isshared(o)) {
		pthread_mutex_unlock(&o->o_mutex);
	}
  }
  return 1;
#endif
}

t_boolean
o_start_write(register t_object *o)
{
#ifdef TEST_MARSHALL
  return 0;
#else
  if (o_isshared(o)) {
	pthread_mutex_lock(&o->o_mutex);
	if (! o_isshared(o)) {
		pthread_mutex_unlock(&o->o_mutex);
	}
  }
  return 1;
#endif
}

void
o_end_read(register t_object *o)
{
  if (o_isshared(o)) pthread_mutex_unlock(&o->o_mutex);
}

void
o_end_write(register t_object *o, int written)
{
  if (o_isshared(o)) {
  	pthread_mutex_unlock(&o->o_mutex);
  	if (written) pthread_cond_broadcast(&o->o_cond);
  }
}

int o_free(t_object *o)
{
  if (! o->o_rtsdep) return 0;
  pthread_mutex_lock(&o->o_mutex);
  o->o_refcount--;
  if (o->o_refcount == 0) {
  	pthread_mutex_unlock(&o->o_mutex);
  	o_kill_rtsdep(o);
	return 1;
  }
  else {
  	pthread_mutex_unlock(&o->o_mutex);
  }
  return 0;
}
