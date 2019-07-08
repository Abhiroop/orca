/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* A C C E S S	 P A T T E R N	 G E N E R A T I O N */

/* $Id: strategy.c,v 1.20 1998/04/27 13:15:37 ceriel Exp $ */

/* See the paper "Object Distribution in Orca using Compile-Time and Run-time
   Techniques", by Henri E. Bal and M. Frans Kaashoek.
   This file has one entry point: strategy_def, which is called with
   one parameter, a definition. The parameter is a process, operation,
   or function, and the local access pattern is produced for this parameter.
*/

#include "debug.h"
#include "ansi.h"

#include <stdio.h>
#include <alloc.h>
#include <assert.h>

#include "LLlex.h"
#include "scope.h"
#include "node.h"
#include "type.h"
#include "strategy.h"
#include "chk.h"		/* for "select_base" */
#include "visit.h"
#include "flexarr.h"

_PROTOTYPE(static int strategy_node, (p_node));
_PROTOTYPE(static int has_obj_params, (t_type *));
_PROTOTYPE(static void add_obj, (t_def *));
_PROTOTYPE(static void check_obj_params, (t_dflst, p_node));
_PROTOTYPE(static int strategy_call, (p_node));
_PROTOTYPE(static char *skipuseless, (char *));
_PROTOTYPE(static char *simplify_pat, (char *));
_PROTOTYPE(static void addstrtostr, (char *));

static p_flex	cf;

#define addchtostr(ch)	do { char *p = flex_next(cf); *p = (ch); } while (0)

#define enter_loop()	addchtostr('{')
#define exit_loop()	addchtostr('}')
#define enter_cond()	addchtostr('[')
#define alt_cond()	addchtostr('|')
#define exit_cond()	addchtostr(']')

static void
addstrtostr(s)
	char	*s;
{
	while (*s) {
		char	*p = flex_next(cf);
		*p = *s;
		s++;
	}
}

void
strategy_def(df)
	t_def	*df;
{
	if (! cf) cf = flex_init(sizeof(char), 32);

	switch(df->df_kind) {
	case D_FUNCTION:
		if (df->df_flags & D_GENERICPAR) break;
		/* Fall through */
	case D_PROCESS:
		flex_clear(cf);
		CurrentScope = df->bod_scope;
		visit_ndlist(df->bod_statlist1, Stat|Call, strategy_node, 0);
		if (df->prc_patstr) free(df->prc_patstr);
		addchtostr('\0');
		df->prc_patstr = simplify_pat(flex_base(cf));
		break;

	case D_OPERATION:
		/* operations do not have shared parameters, so no
		   access pattern is required.
		*/
		break;
	}
}

static int
strategy_node(nd)
	p_node	nd;
{
	static int
		first_arrow;

	assert(nd->nd_class & (Stat|Call));

	switch(nd->nd_symb) {
	case FORK:
	case '$':
	case '(':
		return strategy_call(nd);

	case CHECK:
	case ALIAS_CHK:
	case INIT:
		/* compiler added. ignore. */
		return 1;

	case IF:
		/* IF e1 THEN e2 ELSE e3 FI  => e1 [e2 | e3] */
		visit_node(nd->nd_expr, Call, strategy_call, 0);
		enter_cond();
		visit_ndlist(nd->nd_list1, Stat|Call, strategy_node, 0);
		alt_cond();
		visit_ndlist(nd->nd_list2, Stat|Call, strategy_node, 0);
		exit_cond();
		return 1;

	case CASE:
		/* CASE e1 OF e2 or e2 or en ESAC  => e1 [e2 | e3 | en] */
		visit_node(nd->nd_expr, Call, strategy_call, 0);
		enter_cond();
		first_arrow = 1;
		visit_ndlist(nd->nd_list1, Stat|Call, strategy_node, 0);
		alt_cond();
		visit_ndlist(nd->nd_list2, Stat|Call, strategy_node, 0);
		exit_cond();
		return 1;

	case ARROW:
		if (first_arrow) first_arrow = 0;
		else alt_cond();
		return 0;

	case DO:
		/* DO e OD => [{e}] i.e., assume that it is conditional */
		enter_cond();
		enter_loop();
		visit_ndlist(nd->nd_list1, Stat|Call, strategy_node, 0);
		exit_loop();
		alt_cond();
		exit_cond();
		return 1;

	case FOR:
		/* FOR i IN e1 .. e2 DO e3 OD  =>  e1 e2 [{e3}|] */
		visit_node(nd->nd_expr, Call, strategy_call, 0);
		enter_cond();
		enter_loop();
		visit_ndlist(nd->nd_list1, Stat|Call, strategy_node, 0);
		exit_loop();
		alt_cond();
		exit_cond();
		return 1;

	}
	return 0;
}

static int
has_obj_params(tp)
	t_type	*tp;
{
	t_dflst	p;
	t_def	*df;

	def_walklist(param_list_of(tp), p, df) {
		if (is_shared_param(df)
		    && (df->df_type->tp_flags & T_HASOBJ)) return 1;
	}
	return 0;
}

static void
add_obj(df)
	t_def	*df;
{
	assert(df->df_kind == D_VARIABLE);
	if (is_shared_param(df)) {
		t_dflst	l;
		int	cnt = 0;
		t_def	*df1;
		char	buf[20];

		def_walklist(df->df_scope->sc_definedby->df_type->prc_params, l, df1) {
			cnt++;
			if (df == df1) break;
		}
		assert(l);
		sprintf(buf, "#%d", cnt);
		addstrtostr(buf);
		return;
	}
	addchtostr('@');
	addstrtostr(df->df_idf->id_text);
}

static void
check_obj_params(dl, pl)
	t_dflst	dl;
	p_node	pl;
{
	t_dflst	l;
	t_def	*df;
	p_node	nd;
	int	cnt = 0;

	def_walklist(dl, l, df) {
		if (cnt) addchtostr(',');
		cnt++;
		if (is_shared_param(df)
		    && (df->df_type->tp_flags & T_HASOBJ)) {
			nd = select_base(node_getlistel(pl));
			if (nd->nd_class == Def) {
				add_obj(nd->nd_def);
			}
			else addchtostr('?');
		}
		else addchtostr('*');
		pl = node_nextlistel(pl);
	}
}

static int
strategy_call(nd)
	p_node	nd;
{
	p_node	dsg = nd->nd_callee;

	switch (nd->nd_symb) {
	case FORK:
		if (nd->nd_target) {
			visit_node(nd->nd_target, Call, strategy_call, 0);
		}
		/* fall through */
	case '(':
		if (dsg->nd_type == std_type) return 0;
		visit_node(dsg, Call, strategy_call, 0);
		visit_ndlist(nd->nd_parlist, Call, strategy_call, 0);
		if (dsg->nd_class == Def
		    && (dsg->nd_def->df_kind & (D_FUNCTION|D_PROCESS))
		    && has_obj_params(dsg->nd_type)) {
			addchtostr(nd->nd_symb == '(' ? 'f' : 'p');
			addchtostr('-');
			addstrtostr(dsg->nd_def->df_scope->sc_name);
			addchtostr('.');
			addstrtostr(dsg->nd_def->df_idf->id_text);
			addchtostr('(');
			check_obj_params(param_list_of(dsg->nd_type),
					nd->nd_parlist);
			addchtostr(')');
		}
		break;

	case '$':
		visit_node(nd->nd_obj, Call, strategy_call, 0);
		visit_ndlist(nd->nd_parlist, Call, strategy_call, 0);
		if (nd->nd_obj->nd_class == Def
		    && ! is_parameter(nd->nd_obj->nd_def)
		    && CurrentScope->sc_definedby->df_kind != D_PROCESS) {
			/* operation on local object which can never become
			   shared.
			*/
			break;
		}
		dsg = select_base(nd->nd_obj);
		if (dsg->nd_class != Def) break;
		if ((nd->nd_callee->nd_def->df_flags & D_HASREADS) &&
		    (nd->nd_callee->nd_def->df_flags & D_HASWRITES)) {
			addchtostr('[');
			add_obj(dsg->nd_def);
			addchtostr('$');
			addchtostr('R');
			addchtostr('(');
			check_obj_params(param_list_of(nd->nd_callee->nd_type),
					nd->nd_parlist);
			addchtostr(')');
			addchtostr('|');
		}
		add_obj(dsg->nd_def);
		addchtostr('$');
		if (nd->nd_callee->nd_def->df_flags & D_HASWRITES) {
			addchtostr('W');
		}
		else	addchtostr('R');
		addchtostr('(');
		check_obj_params(param_list_of(nd->nd_callee->nd_type),
				nd->nd_parlist);
		addchtostr(')');
		if ((nd->nd_callee->nd_def->df_flags & D_HASREADS) &&
		    (nd->nd_callee->nd_def->df_flags & D_HASWRITES)) {
			addchtostr(']');
		}
		break;
	}
	return 1;
}

static char *
skipuseless(s)
	char	*s;
{
	char	*p = s;

	if (*p == '{') {
		p++;
		while (*p == '{' || *p == '[') {
			char *p1 = skipuseless(p);
			if (p1 != p) {
				p = p1;
				continue;
			}
			break;
		}
		if (*p == '}') return p+1;
	}
	else {
		p++;
		for (;;) {
			while (*p == '{' || *p == '[') {
				char *p1 = skipuseless(p);
				if (p1 != p) {
					p = p1;
					continue;
				}
				break;
			}
			if (*p == ']') return p+1;
			if (*p == '|') {
				p++;
				continue;
			}
			break;
		}
	}
	return s;
}

static char *
simplify_pat(p)
	char	*p;
{
	/*	Walk through a pattern string, skipping the useless
		information.
	*/
	p_flex	f;
	char	*q;

	f = flex_init(sizeof(char), 32);
	if (p) {
	    while (*p) {
		if (*p == '[' || *p == '{') {
			char *p1 = skipuseless(p);
			if (p1 != p) {
				p = p1;
				continue;
			}
		}
		q = flex_next(f);
		*q = *p++;
	    }
	}
	q = flex_next(f);
	*q = '\0';
	return flex_finish(f, (unsigned int *) 0);
}
