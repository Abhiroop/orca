/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: misc.c,v 1.8 1998/01/21 10:58:32 ceriel Exp $ */

#include <interface.h>

int cmp_string(void *aa, void *bb) {
    t_string *a = aa, *b = bb;
    if (a == b) return 1;
    if (a->a_sz <= 0) return b->a_sz <= 0;
    if (a->a_dims[0].a_lwb != b->a_dims[0].a_lwb) return 0;
    if (a->a_dims[0].a_nel != b->a_dims[0].a_nel) return 0;
    return memcmp((char *) a->a_data + a->a_offset,
		  (char *) b->a_data + b->a_offset,
		  a->a_sz) == 0;
}

void ass_string(void *dd, void *ss)
{
  t_string *dst = dd, *src = ss;
  size_t off = src->a_offset;
  char *q = (char *)(src->a_data) + off;

  if (dst == src) return;
  if (dst->a_sz != src->a_sz) {
	if (dst->a_sz > 0) m_free((char *)(dst->a_data) + dst->a_offset);
	dst->a_sz = src->a_sz;
	if (src->a_sz <= 0) return;
	dst->a_data = (char *)m_malloc(src->a_sz) - off;
  }
  dst->a_offset = src->a_offset;
  dst->a_dims[0].a_lwb = src->a_dims[0].a_lwb;
  dst->a_dims[0].a_nel = src->a_dims[0].a_nel;
  if (src->a_sz <= 0) return;
  memcpy(dst->a_data + off, q, src->a_sz);
}

void free_string(void *s)
{
  t_string *a = s;
  if (a->a_sz <= 0) return;
  m_free((char *)(a->a_data) + a->a_offset);
}

int cmp_enum(void *a, void *b)
{
	return *(t_enum *)a == *(t_enum *)b;
}

void ass_enum(void *a, void *b)
{
	*(t_enum *)a = *(t_enum *)b;
}

int cmp_longenum(void *a, void *b)
{
	return *(t_longenum *)a == *(t_longenum *)b;
}

void ass_longenum(void *a, void *b)
{
	*(t_longenum *)a = *(t_longenum *)b;
}

int cmp_integer(void *a, void *b)
{
	return *(t_integer *)a == *(t_integer *)b;
}

void ass_integer(void *a, void *b)
{
	*(t_integer *)a = *(t_integer *)b;
}

int cmp_longint(void *a, void *b)
{
	return *(t_longint *)a == *(t_longint *)b;
}

void ass_longint(void *a, void *b)
{
	*(t_longint *)a = *(t_longint *)b;
}

int cmp_shortint(void *a, void *b)
{
	return *(t_shortint *)a == *(t_shortint *)b;
}

void ass_shortint(void *a, void *b)
{
	*(t_shortint *)a = *(t_shortint *)b;
}

int cmp_real(void *a, void *b)
{
	return *(t_real *)a == *(t_real *)b;
}

void ass_real(void *a, void *b)
{
	*(t_real *)a = *(t_real *)b;
}

int cmp_longreal(void *a, void *b)
{
	return *(t_longreal *)a == *(t_longreal *)b;
}

void ass_longreal(void *a, void *b)
{
	*(t_longreal *)a = *(t_longreal *)b;
}

int cmp_shortreal(void *a, void *b)
{
	return *(t_shortreal *)a == *(t_shortreal *)b;
}

void ass_shortreal(void *a, void *b)
{
	*(t_shortreal *)a = *(t_shortreal *)b;
}

int (cmp_nodename)(void *a, void *b)
{
	return cmp_nodename((t_nodename *)a,(t_nodename *)b);
}

void ass_nodename(void *a, void *b)
{
#ifndef NO_AGE
	((t_nodename *)a)->n_age = ((t_nodename *)b)->n_age;
	((t_nodename *)a)->n_index = ((t_nodename *)b)->n_index;
#else
	*(t_nodename *)a = *(t_nodename *)b;
#endif
}

#ifdef SHMEM
void m_shareprop(void *p, tp_dscr *d, int sh)
/* Propagate shared memory information 'sh' over the datastructure indicated
   by 'p'. This structure has type 'd'.
*/
{
  switch(d->td_type) {
  case RECORD: {
	register fld_dscr *f = td_fields(d);
	register int nf = td_nfields(d);
	while (nf) {
		if (f->fld_dscr->td_flags & DYNAMIC) {
			m_shareprop((char *)p+f->fld_offset, f->fld_dscr, sh);
		}
		nf--;
		f++;
	}
	break;
  }
  case UNION:
	((t_union *) p)->u_shared = sh;
	break;

  case ARRAY: 
	((t_array *) p)->a_shared = sh;
	break;

  case SET:
	((t_set *) p)->s_shared = sh;
	break;

  case BAG:
	((t_bag *) p)->b_shared = sh;
	break;

  case GRAPH:
	((t_graph *) p)->g_shared = sh;
	break;
}
#endif
