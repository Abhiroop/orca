/* $Id: DoOperation.c,v 1.8 1996/07/04 08:53:47 ceriel Exp $ */

#include <interface.h>

void
DoOperation(register t_object *o, int *op_flags, tp_dscr *d, int opindex, int att, void **argtab)
{
  register t_object *tmp;
  register struct op_descr *op;
  register void **p;
  register int npars;
  
  if (*op_flags & BLOCKING) return;

  op = td_operations(d);
  op = &op[opindex];

  if (op->op_read_alts) {
  	if ((*op->op_read_alts)(o, argtab)) {
  		if (op->op_write_alts) {
  			if ((*op->op_write_alts)(o, argtab)) {
				*op_flags |= BLOCKING;
			}
		}
		else *op_flags |= BLOCKING;
  	}
  }
  else {
  	if ((*op->op_write_alts)(o, argtab)) {
		*op_flags |= BLOCKING;
	}
  }
  if (*op_flags & BLOCKING) {
	if (! (*op_flags & NESTED)) {
  		m_trap(LOCAL_DEADLOCK, (char *) 0, 0);
	}
  }
}

int o_free(t_object *o)
{
  o->o_refcount--;
  if (o->o_refcount == 0) {
  	o_kill_rtsdep(o);
	return 1;
  }
  o->o_fields = 0;
  return 0;
}

