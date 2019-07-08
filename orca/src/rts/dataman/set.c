/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: set.c,v 1.9 1996/07/04 08:51:52 ceriel Exp $ */

#include <interface.h>

void s_addel(t_set *s, tp_dscr *d, void *el)
{
  t_elem *espot = 0, *e = s->s_elem;
  int i, spot = 0, ispot = 0;
  tp_dscr *n = td_elemdscr(d);
  void *p;

  while (e) {
	for (i = 0; i < MAXELC; i++) {
		int off = i * n->td_size;
		if (e->e_mask & (1 << i)) {
			if ((*td_elcmp(d))(e->e_buf+off, el)) return;
		}
		else if (! espot) {
			espot = e;
			spot = off;
			ispot = i;
		}
		else if ((unsigned)(e->e_mask) < (1 << i)) break;
	}
	e = e->e_next;
  }
  if (! espot) {
	espot = m_malloc(sizeof(t_elemhdr)+MAXELC * n->td_size);
	espot->e_next = s->s_elem;
	s->s_elem = espot;
	espot->e_mask = 0;
	espot->e_count = 0;
  }
  p = espot->e_buf + spot;
  espot->e_mask |= (1 << ispot);
  espot->e_count++;

  if (n->td_flags & DYNAMIC) {
	memset(p, '\0', n->td_size);
	(*td_elassign(d))(p, el);
  }
  else memcpy(p, el, n->td_size);
  s->s_nelem++;
}

int s_delel(t_set *s, tp_dscr *d, void *el)
{
  tp_dscr *n = td_elemdscr(d);
  t_elem *e = s->s_elem, *prev = 0;
  void *p;
  int i;

  while (e) {
	for (i = 0; i < MAXELC; i++) {
		if (e->e_mask & (1 << i)) {
			p = e->e_buf+i*n->td_size;
			if ((*td_elcmp(d))(p, el)) {
				s->s_nelem--;
				e->e_count--;
				e->e_mask &= ~(1 << i);
				if (n->td_flags & DYNAMIC) {
					(*(td_elfreefunc(d)))(p);
				}
				if (e->e_count == 0) {
					if (prev) prev->e_next = e->e_next;
					else s->s_elem = e->e_next;
  					m_free(e);
				}
				return 1;
			}
		}
	}
	prev = e;
	e = e->e_next;
  }
  return 0;
}

t_boolean s_member(t_set *s, tp_dscr *d, void *el)
{
  t_elem *e = s->s_elem;
  tp_dscr *n;
  int i;

  n = td_elemdscr(d);
  while (e) {
	int hit = 0;
	for (i = 0; i < MAXELC; i++) {
		if (e->e_mask & (1 << i)) {
			hit++;
			if ((*td_elcmp(d))(e->e_buf+i*n->td_size, el)) return 1;
		}
		if (hit >= e->e_count) break;
	}
	e = e->e_next;
  }
  return 0;
}

void s_from(t_set *s, tp_dscr *d, void *result)
{
  t_elem *e = s->s_elem;
  tp_dscr *n = td_elemdscr(d);
  int i;

  for (i = 0; i < MAXELC; i++) {
	if (e->e_mask & (1 << i)) break;
  }
  assert(i < MAXELC);
  s->s_nelem--;
  e->e_count--;
  e->e_mask &= ~(1 << i);
  memcpy(result, e->e_buf+i*n->td_size, n->td_size);
  if (e->e_mask == 0) {
	s->s_elem = e->e_next;
	m_free(e);
  }
}

void s_add(t_set *a, t_set *b, tp_dscr *d)
{
  t_elem *e;
  tp_dscr *n = td_elemdscr(d);
  int i;

  for (e = b->s_elem; e; e = e->e_next) {
	int hit = 0;
	for (i = 0; i < MAXELC; i++) {
		if (e->e_mask & (1 << i)) {
			hit++;
			s_addel(a, d, e->e_buf + i * n->td_size);
		}
		if (hit >= e->e_count) break;
	}
  }
}

void s_sub(t_set *a, t_set *b, tp_dscr *d)
{
  t_elem *e;
  tp_dscr *n = td_elemdscr(d);
  int i;

  for (e = b->s_elem; e; e = e->e_next) {
	int hit = 0;
	for (i = 0; i < MAXELC; i++) {
		if (e->e_mask & (1 << i)) {
			hit++;
			s_delel(a, d, e->e_buf + i * n->td_size);
		}
		if (hit >= e->e_count) break;
	}
  }
}

void s_inter(t_set *a, t_set *b, tp_dscr *d)
{
  t_elem *e = a->s_elem, *prev = 0;
  int i;
  tp_dscr *n = td_elemdscr(d);

  while (e) {
	for (i = 0; i < MAXELC; i++) {
		if (e->e_mask & (1 << i)) {
			void *p = e->e_buf+i*n->td_size;
			if (! s_member(b, d, p)) {
				a->s_nelem--;
				e->e_count--;
				e->e_mask &= ~(1 << i);
				if (n->td_flags & DYNAMIC) {
					(*(td_elfreefunc(d)))(p);
				}
				if (e->e_count == 0) {
					if (prev) prev->e_next = e->e_next;
					else a->s_elem = e->e_next;
  					m_free(e);
					if (prev) e = prev->e_next;
					else e = a->s_elem;
					break;
				}
			}
		}
	}
	if (i == MAXELC) {
		prev = e;
		e = e->e_next;
	}
  }
}

void s_symdiff(t_set *a, t_set *b, tp_dscr *d)
{
  t_set	c;

  c.s_elem = 0;
  c.s_nelem = 0;
  (*td_sassign(d))(&c, b);
  s_sub(&c, a, d);
  s_sub(a, b, d);
  s_add(a, &c, d);
  (*td_freefunc(d))(&c);
}
