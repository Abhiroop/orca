/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: visit.c,v 1.15 1997/05/15 12:03:23 ceriel Exp $ */

/* Tree walker. */

#include <stdio.h>

#include "debug.h"
#include "ansi.h"

#include <assert.h>

#include "visit.h"

void
visit_ndlist(l, kinds, func, bottom_up)
	p_node	l;
	int	kinds;
	int	bottom_up;
#if __STDC__
	int	(*func)(p_node);
#else
	int	(*func)();
#endif
{
	p_node	l1;
	p_node	nd;

	node_walklist(l, l1, nd) {
		visit_node(nd, kinds, func, bottom_up);
	}
}

void
visit_node(nd, kinds, func, bottom_up)
	p_node	nd;
	int	kinds;
	int	bottom_up;
#if __STDC__
	int	(*func)(p_node);
#else
	int	(*func)();
#endif
{
	if (! nd) return;

	if (! bottom_up && (nd->nd_class & kinds)) {
		if ((*func)(nd)) return;
	}
	switch(nd->nd_class) {
	case Oper:
	case Arrsel:
	case Select:
	case Link:
	case Check:
	case Ofldsel:
		visit_node(nd->nd_left, kinds, func, bottom_up);
		/* fall through */
	case Uoper:
		visit_node(nd->nd_right, kinds, func, bottom_up);
		break;
	case Call:
		visit_node(nd->nd_callee, kinds, func, bottom_up);
		if (nd->nd_symb == '$') {
			visit_node(nd->nd_obj, kinds, func, bottom_up);
		}
		if (nd->nd_symb == FORK) {
			visit_node(nd->nd_target, kinds, func, bottom_up);
		}
		visit_ndlist(nd->nd_parlist, kinds, func, bottom_up);
		break;
	case Aggr:
	case Row:
		visit_ndlist(nd->nd_memlist, kinds, func, bottom_up);
		break;
	case Stat:
		visit_node(nd->nd_expr, kinds, func, bottom_up);
		visit_node(nd->nd_desig, kinds, func, bottom_up);
		visit_ndlist(nd->nd_list1, kinds, func, bottom_up);
		visit_ndlist(nd->nd_list2, kinds, func, bottom_up);
		break;
	}
	if (bottom_up && (nd->nd_class & kinds)) {
		(void) ((*func)(nd));
	}
}
