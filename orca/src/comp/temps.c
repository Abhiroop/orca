/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/*  T E M P O R A R Y	V A R I A B L E S  */

/* $Id: temps.c,v 1.12 1997/05/15 12:03:08 ceriel Exp $ */

#include "ansi.h"
#include "debug.h"

#include <assert.h>
#include <alloc.h>

#include "temps.h"
#include "scope.h"
#include "def.h"

static t_tmp
	*tmp_list;
static int
	tmp_idcnt = 1;
static t_def
	*currdef;

t_tmp *
newtemp(tp, isptr)
	t_type	*tp;
	int	isptr;
{
	/*	Get a temporary variable, which has type 'tp'. If 'isptr' is
		set, it has type "ptr to 'tp'" instead.
	*/

	t_tmp	*t = tmp_list;
	t_tmp	*prev = 0;

	while (t && ! (t->tmp_type == tp && t->tmp_ispointer == isptr)) {
		prev = t;
		t = t->tmp_next;
	}
	if (t) {
		if (prev) prev->tmp_next = t->tmp_next;
		else tmp_list = t->tmp_next;
		return t;
	}
	t = new_tmp();
	t->tmp_id = tmp_idcnt++;
	t->tmp_type = tp;
	t->tmp_ispointer = isptr;
	t->tmp_scopelink = currdef->bod_temps;
	currdef->bod_temps = t;
	return t;
}

void
oldtemp(t)
	t_tmp	*t;
{
	/*	Release the temporary indicated by 't'.
	*/

	t_tmp	*p = tmp_list;

	while (p) {
		if (p == t) return;
		p = p->tmp_next;
	}
	t->tmp_next = tmp_list;
	tmp_list = t;
}

void
inittemps()
{
	/*	Initialize for before allocating temporaries. This
		routine must be called before allocating temporaries
		for every routine, operation or function.
	*/
	t_def	*df = CurrentScope->sc_definedby;

	currdef = df;
	if (df->bod_temps) {
		tmp_idcnt = df->bod_temps->tmp_id + 1;
	}
	else	tmp_idcnt = 1;
	tmp_list = 0;
}

p_node
get_tmpvar(tp, ptr)
	t_type	*tp;
	int	ptr;
{
	/*	Create a node indicating a new temporary variable.
	*/

	t_tmp	*t = newtemp(tp, ptr);
	p_node	nd = mk_leaf(Tmp, IDENT);

	nd->nd_tmpvar = t;
	nd->nd_type = tp;
	return nd;
}
