/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: alias.c,v 1.7 1996/07/04 08:52:31 ceriel Exp $ */

#include <interface.h>

/* Anti-aliasing stuff.
   The compiler generates a call to "m_aliaschk" whenever it is unable
   to determine if a certain function call contains parameters that are
   aliases of each other. (E.g.,: foo(A[expr1], A[expr2]).
*/

static void do_chk(void *a, void *b, tp_dscr *da, char *fn, int ln);
static void chk_array(t_array *a, void *addr, tp_dscr *da, char *fn, int ln);
static void chk_graph(t_graph *a, void *addr, tp_dscr *da, char *fn, int ln);
static void chk_union(t_union *a, void *addr, tp_dscr *da, char *fn, int ln);
static void chk_record(char *a, void *addr, tp_dscr *da, char *fn, int ln);

void
m_aliaschk(void *a, void *b, tp_dscr *da, tp_dscr *db, char *fn, int ln)
{
	if (a == b) m_trap(ALIAS, fn, ln);
	do_chk(a, b, da, fn, ln);
	do_chk(b, a, db, fn, ln);
}

static void
do_chk(void *a, void *b, tp_dscr *da, char *fn, int ln)
{
	switch(da->td_type) {
	case RECORD:
		chk_record((char *)a, b, da, fn, ln);
		break;
	case GRAPH:
		chk_graph((t_graph *)a, b, da, fn, ln);
		break;
	case UNION:
		chk_union((t_union *)a, b, da, fn, ln);
		break;
	case ARRAY:
		chk_array((t_array *)a, b, da, fn, ln);
		break;
	}
}

static void
chk_array(t_array *a, void *addr, register tp_dscr *d, char *fn, int ln)
{
	register char *p = a->a_data;

	d = td_elemdscr(d);
	if (! p) return;
	if ((char *)addr >= &p[a->a_offset*d->td_size]
	    && (char *)addr < &p[(a->a_offset+a->a_sz)*d->td_size]) {
		m_trap(ALIAS, fn, ln);
	}
	if (d->td_flags & DYNAMIC) {
		register int i = a->a_sz;
		p = &p[a->a_offset*d->td_size];
		while (i-- > 0) {
			do_chk(p, addr, d, fn, ln);
			p += d->td_size;
		}
	}
}

static void
chk_record(char *a, void *addr, register tp_dscr *d, char *fn, int ln)
{
	if ((char *)addr >= a && (char *)addr < &a[d->td_size]) m_trap(ALIAS, fn, ln);
	if (d->td_flags & DYNAMIC) {
		register fld_dscr *f = td_fields(d);
		register int i = td_nfields(d);

		while (i-- > 0) {
			if (f->fld_descr->td_flags & DYNAMIC) {
				do_chk(&a[f->fld_offset], addr, f->fld_descr, fn, ln);
			}
			f++;
		}
	}
}

static void
chk_union(t_union *u, void *addr, register tp_dscr *d, char *fn, int ln)
{
	register var_dscr *v;
	register int i;

	if ((char *)addr >= (char *)u
	    && (char *)addr <= &((char *)u)[d->td_size]) {
		m_trap(ALIAS, fn, ln);
	}
	if (! u->u_init || !(d->td_flags & DYNAMIC)) return;
	v = td_variants(d);
	i = td_nvariants(d);
	while (i-- > 0) {
		if (v->var_tagval == u->u_tagval) {
			if (v->var_descr->td_flags & DYNAMIC) {
				do_chk(&((char *) u)[v->var_offset], addr,
					v->var_descr, fn, ln);
			}
			break;
		}
		v++;
	}
}

static void
chk_graph(t_graph *g, void *addr, register tp_dscr *d, char *fn, int ln)
{
	register int sz = g->g_size;
	register t_mt *m = g->g_mt;

	chk_record(&((char *) g)[td_rootoff(d)], addr, td_rootdscr(d), fn, ln);
	d = td_nodedscr(d);
	while (sz-- > 0) {
		if (! nodeisfree(m)) {
			chk_record((char *)(m->g_node), addr, d, fn, ln);
		}
		m++;
	}
}
