/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: array.c,v 1.13 1997/01/07 16:32:06 ceriel Exp $ */

#include <interface.h>

void a_allocate(void *arp, int ndim, size_t elsz, ...)
{
  register int i;
  register int sz = 1;
  register int offset = 0;
  register t_array *a = arp;
  va_list ap;
  void *p;

  va_start(ap, elsz);

  for (i = 0; i < ndim; i++) {
	int	lb, ub;

	lb = va_arg(ap, int);
	ub = va_arg(ap, int);
	a->a_dims[i].a_lwb = lb;
	if (ub < lb) {
		sz = 0;
		ub = lb - 1;
	}
	a->a_dims[i].a_nel = ub - lb + 1;
	if (i == 0) {
		sz = (ub - lb + 1);
	}
	else {
		sz *= (ub - lb + 1);
		offset *= (ub - lb + 1);
	}
	offset += lb;
  }

  if (sz <= 0) {
	a->a_data = 0;
	a->a_offset = 0;
	a->a_sz = 0;
	return;
  }

  a->a_sz = sz;
  a->a_offset = offset;

  p = m_malloc(a->a_sz * elsz);
  if (offset == 0) a->a_data = (char *) p;
  else a->a_data = (char *) p - elsz*offset;
}
