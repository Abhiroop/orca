/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/*   P R E P A R A T I O N   */

/* $Id: prepare.c,v 1.31 1998/02/27 12:03:23 ceriel Exp $ */

/* This file contains routines to prepare the parse tree for optimization and
   code generation. The AND and OR operators are rewritten as ANDBECOMES and
   ORBECOMES, and set operations are rewritten as well.
   Also, checks are added as required.
*/

#include "ansi.h"
#include "debug.h"

#include <assert.h>
#include <alloc.h>
#include <stdio.h>

#include "prepare.h"
#include "scope.h"
#include "node.h"
#include "type.h"
#include "chk.h"
#include "visit.h"
#include "generate.h"
#include "simplify.h"
#include "temps.h"
#include "options.h"
#include "opt_SR.h"
#include "oc_stds.h"
#include "error.h"
#include "main.h"

static p_node	new_stats;

_PROTOTYPE(static p_node prepare_list, (p_node, int, int));
_PROTOTYPE(static void prepare, (p_node));
_PROTOTYPE(static int markcopy, (p_node));
_PROTOTYPE(static void prepare_bats, (t_def *));
_PROTOTYPE(static void prepare_logic, (p_node));
_PROTOTYPE(static void prepare_sets, (p_node));
_PROTOTYPE(static void copy_formals, (t_def *));
_PROTOTYPE(static void mk_check, (p_node, int));
_PROTOTYPE(static void aliases, (p_node, p_node, t_dflst));
_PROTOTYPE(static void simple_desig, (p_node));
_PROTOTYPE(static void simple_expr, (p_node));
_PROTOTYPE(static void simple_bats, (p_node));
_PROTOTYPE(static int SR_part, (p_node));
_PROTOTYPE(static int has_side_effects, (p_node));
_PROTOTYPE(static int chk_side_effects, (p_node));

static void
copy_formals(fnc)
	t_def	*fnc;
{
	/*	Check which formals must be copied, and set D_COPY
		for them. A copy must be made if the formal is an
		IN parameter and it is changed, and it is not a simple
		type (or we already are dealing with a copy), or we
		need the old value as well (to restore it in case of a
		blocking operation).
		If the type contains objects, we are already operating on
		a copy.
	*/
	t_def	*df;
	t_dflst	l;
	int	blocking_operation =
		  fnc->df_kind == D_OPERATION && (fnc->df_flags & D_BLOCKING);

	def_walklist(fnc->df_type->prc_params, l, df) {
		if (is_in_param(df)
		    && (df->df_flags & D_DEFINED)
		    && (blocking_operation
			|| (is_constructed_type(df->df_type)
			    && ! (df->df_type->tp_flags & T_HASOBJ)))) {
			df->df_flags |= D_COPY;
		}
	}
}

static int
markcopy(nd)
	p_node	nd;
{
	/* In general, we would not know in chk.c (is_operation), whether
	   an operation does write or read. Therefore, D_DEFINED marking
	   is postponed until here.
	*/

	if (nd->nd_symb == '$'
	    && (nd->nd_callee->nd_def->df_flags & D_HASWRITES)) {
		mark_defs(nd->nd_obj, D_DEFINED);
	}
	return 0;
}

void
prepare_df(df)
	t_def	*df;
{
	/*	Prepare the body of df.
	*/

	t_scope	*sv_scope = CurrentScope;

	assert(df->df_kind & (D_MODULE|D_OBJECT|D_PROCESS|D_FUNCTION|D_OPERATION));

	CurrentScope = df->bod_scope;
	inittemps();
	if (df->df_kind & (D_FUNCTION|D_PROCESS|D_OPERATION)) {
		visit_ndlist(df->bod_statlist1, Call, markcopy, 0);
		visit_ndlist(df->bod_statlist2, Call, markcopy, 0);
		copy_formals(df);
	}
	node_initlist(&new_stats);
	walkdefs(CurrentScope->sc_def,
		 D_TYPE|D_VARIABLE|D_OFIELD,
		 prepare_bats);
	if (! node_emptylist(new_stats)) {
		node_enlist(&new_stats, mk_leaf(Stat, 0));
	}
	df->bod_init = new_stats;
	node_initlist(&new_stats);
	df->bod_statlist1 = prepare_list(df->bod_statlist1, 1, 1);
	df->bod_statlist2 = prepare_list(df->bod_statlist2, 1, 1);
	if (df->df_kind == D_OPERATION && df->opr_dependencies) {
		df->opr_dependencies = prepare_list(df->opr_dependencies, 1, 1);
	}
	CurrentScope = sv_scope;
}

static p_node
prepare_list(l, statlevel, place_holder)
	p_node	l;
	int	statlevel;
	int	place_holder;
{
	/*	Prepare the statement list in l and return the
		(possibly modified) list.
		If 'statlevel' is set, 'l' indicates a statement list,
		and we can add assignments, checks, etc. The global
		variable 'new_stats' is used for this.
	*/

	p_node	ll,
		new_ll,		/* only if statlevel is not set. */
		sv_new_stats = new_stats;
	p_node	nd;

	if (place_holder) {
		/* Add place holder for optimization sets. */
		node_enlist(&l, mk_leaf(Stat, 0));
	}

	node_initlist(&new_ll);

	/* Initialize new list. */
	if (statlevel) {
		node_initlist(&new_stats);
	}

	/* Walk through old list, creating new list. */
	node_walklist(l, ll, nd) {
		prepare(nd);
		if (statlevel) {
			node_enlist(&new_stats, nd);
		}
		else {
			node_enlist(&new_ll, nd);
		}
	}

	if (statlevel) {
		node_walklist(l, ll, nd) {
			if ((nd->nd_symb == BECOMES ||
			     nd->nd_symb == TMPBECOMES) &&
			    nd->nd_desig->nd_class == Tmp) {
				oldtemp(nd->nd_desig->nd_tmpvar);
			}
		}
		new_ll = new_stats;
		new_stats = sv_new_stats;
	}
	return new_ll;
}

static void
prepare(nd)
	p_node	nd;
{
	/*	Add checks for node 'nd'.
	*/

	p_node	e;
	p_node	l;
	int	i;

	if (! nd) return;

	switch (nd->nd_class) {
	case Uoper:
		if (nd->nd_symb == ARR_SIZE) {
			/* In this case, the expression is already prepared.
			   (See the code at case Arrsel: ).
			*/
			break;

		}
		prepare(nd->nd_right);
		if (nd->nd_symb == FROM) {
			if (! options['c']) {
				simple_desig(nd->nd_right);
				mk_check(nd, FROM_CHECK);
			}
		}
		break;

	case Link:
		prepare(nd->nd_left);
		prepare(nd->nd_right);
		break;

	case Oper:
		if (nd->nd_symb == AND || nd->nd_symb == OR) {
			prepare_logic(nd);
			break;
		}
		prepare(nd->nd_right);
		if (nd->nd_symb == ARR_INDEX) {
			e = nd->nd_right;
			if (! options['c']) {
				if (! nd->nd_left && e->nd_class == Def &&
				    (e->nd_def->df_flags & D_PART_INDEX) &&
				    nd->nd_dimno == e->nd_def->var_level) {
				}
				else {
					simple_expr(e);
					mk_check(nd, A_CHECK);
				}
			}
			*nd = *e;
			free_node(e);
			break;
		}
		prepare(nd->nd_left);
		if (nd->nd_type->tp_fund & (T_SET|T_BAG)) {
			prepare_sets(nd);
		}
		if (nd->nd_symb == '/' || nd->nd_symb == '%') {
			simple_expr(nd->nd_right);
			mk_check(nd,
				 (nd->nd_symb=='/') ? DIV_CHECK : MOD_CHECK);
		}
		break;

	case Arrsel:
		prepare(nd->nd_left);

		if (nd->nd_left->nd_type->tp_fund == T_GRAPH) {
			prepare(nd->nd_right);
			/* If this is a graph node selection, we must be able to
			   designate the nodename. Otherwise, the array indexes
			   must be simple expressions, because they may be
			   evaluated more than once (for array bound checking).
			*/
			if (! options['c']) {
				simple_desig(nd->nd_left);
				simple_desig(nd->nd_right);
				mk_check(nd, G_CHECK);
				break;
			}
			break;
		}
		/* Fall through */

	case Ofldsel:

		/* Fix the nd_left references in ARR_INDEX nodes
		   and the nd_right references in ARR_SIZE nodes.
		*/
		assert(nd->nd_class == Ofldsel ||
		       nd->nd_left->nd_type->tp_fund == T_ARRAY);

		if (nd->nd_class == Ofldsel) {
			i = CurrDef->df_type->arr_ndim;
		}
		else	i = nd->nd_left->nd_type->arr_ndim;
		if (! options['c'] || i > 1) {
			if (nd->nd_left->nd_type->tp_fund != T_ARRAY ||
			    !(nd->nd_left->nd_type->tp_flags & T_CONSTBNDS)) {
				simple_desig(nd->nd_left);
			}
		}
		e = nd->nd_right;

		while (e) {
			switch(e->nd_symb) {
			case '*':
				assert(e->nd_right->nd_symb == ARR_SIZE);
				if (nd->nd_class == Arrsel) {
					e->nd_right->nd_right =
						node_copy(nd->nd_left);
				}
				e = e->nd_left;
				break;
			case '+':
				assert(e->nd_right->nd_symb == ARR_INDEX ||
				       options['O']);
				if (nd->nd_class == Arrsel &&
				    e->nd_right->nd_symb == ARR_INDEX &&
				    ! options['c']) {
					e->nd_right->nd_left =
						node_copy(nd->nd_left);
				}
				e = e->nd_left;
				break;
			case ARR_INDEX:
				if (nd->nd_class == Arrsel && ! options['c']) {
					e->nd_left = node_copy(nd->nd_left);
				}
				e = 0;
				break;
			default:
				assert(options['O']);
				e = 0;
				break;
			}
		}

		prepare(nd->nd_right);
		break;

	case Aggr:
	case Row:
		nd->nd_memlist = prepare_list(nd->nd_memlist, 0, 0);
		break;

	case Select:
		/* Graph, record, or union selection. */
		prepare(nd->nd_left);
		prepare(nd->nd_right);
		if (! options['c'] &&
		    nd->nd_left->nd_type->tp_fund == T_UNION) {
			/* For a union selection, add a check, but only if
			   it is one of the variants, not if it is the
			   union tag.
			*/
			assert(nd->nd_right->nd_class == Def);
			if (! (nd->nd_right->nd_def->df_flags & D_TAG)) {
				simple_desig(nd->nd_left);
				mk_check(nd, U_CHECK);
			}
		}
		if (nd->nd_symb == '!') {
			/* a!b is a short-hand for a[a.b]. Transform tree.
			*/
			p_node	arg = mk_leaf(Select, '.');

			arg->nd_pos = nd->nd_pos;
			mk_simple_desig(nd->nd_left, &new_stats);
			arg->nd_left = node_copy(nd->nd_left);
			arg->nd_right = nd->nd_right;
			arg->nd_type = nd->nd_type;
			nd->nd_type = nd->nd_def->df_type;
			nd->nd_class = Arrsel;
			nd->nd_symb = '[';
			nd->nd_right = arg;
			if (! options['c']) {
				mk_check(nd, G_CHECK);
			}
		}
		break;

	case Stat:
		prepare(nd->nd_desig);
		switch(nd->nd_symb) {
		case COND_EXIT:
			if (nd->nd_expr->nd_symb == OR) {
				/* COND_EXIT A OR B becomes:
					COND_EXIT A
					COND_EXIT B
				*/
				p_node	n = mk_leaf(Stat, COND_EXIT);
				n->nd_pos = nd->nd_pos;
				n->nd_expr = nd->nd_expr->nd_left;
				prepare(n);
				node_enlist(&new_stats, n);
				n = nd->nd_expr;
				nd->nd_expr = nd->nd_expr->nd_right;
				free_node(n);
				prepare(nd);
				return;
			}
			if (nd->nd_expr->nd_symb == AND) {
				/* COND_EXIT A AND B becomes:
					IF A THEN
						COND_EXIT B
					FI
				*/
				p_node	n = mk_leaf(Stat, COND_EXIT);
				e = nd->nd_expr;
				n->nd_pos = nd->nd_pos;
				n->nd_expr = e->nd_right;
				nd->nd_symb = IF;
				nd->nd_expr = e->nd_left;
				free_node(e);
				node_enlist(&nd->nd_list1, n);
				prepare(nd);
				return;
			}
			break;
		case ANDBECOMES:
		case ORBECOMES:
			prepare_logic(nd);
			return;
		case BECOMES:
			/* Special treatment for a := b + c when this is
			   a set operation, to prevent extra temporary.
			*/
			if (nd->nd_expr->nd_class == Oper &&
			    (nd->nd_expr->nd_type->tp_fund & (T_BAG|T_SET))) {
				prepare_sets(nd);
				return;
			}
			break;
		case FOR: {
			/* Allocate temporaries for the FOR-loop. */
			t_def *loopid;
			t_tmp *t;

			loopid = nd->nd_desig->nd_def;
			prepare(nd->nd_expr);

			e = nd->nd_expr;
			if (e->nd_class == Link) {
				/* Loop over a range. */

				if (! (e->nd_right->nd_flags & ND_CONST)) {
					/* Temporary for upper bound. */
					mk_temp_expr(e->nd_right, 0, &new_stats);
				}
				/* Temporary for loop. */
				mk_temp_expr(e->nd_left, 0, &new_stats);
				t = e->nd_left->nd_tmpvar;
			}
			else {	/* Loop over a set or bag. */
				/* Temporary copy of set or bag. */
				mk_temp_expr(e, 0, &new_stats);
				/* Temporary for loop. */
				t = newtemp(element_type_of(e->nd_type), 0);
			}
			loopid->var_tmpvar = t;

			nd->nd_list1 = prepare_list(nd->nd_list1, 1, 1);
			oldtemp(t);
			if (e->nd_class == Link) {
				if (! (e->nd_right->nd_flags & ND_CONST)) {
					oldtemp(e->nd_right->nd_tmpvar);
				}
			}
			else {
				oldtemp(e->nd_tmpvar);
			}
			}
			return;
		case ACCESS:
			node_walklist(nd->nd_list1, l, e) {
				prepare(e);
			}
			return;
		case IF:
			e = nd->nd_expr;
			if (e->nd_symb == AND && node_emptylist(nd->nd_list2)) {
				/* IF A AND B THEN S FI becomes:
					IF A THEN IF B THEN S FI FI
				*/
				p_node	n = mk_leaf(Stat, IF);
				n->nd_pos = nd->nd_pos;
				n->nd_expr = e->nd_right;
				nd->nd_expr = e->nd_left;
				free_node(e);
				n->nd_list1 = nd->nd_list1;
				node_initlist(&nd->nd_list1);
				node_enlist(&nd->nd_list1, n);
				prepare(nd);
				return;
			}
			/* Fall through */
		case DO:
		case GUARD:
			prepare(nd->nd_expr);
			nd->nd_list1 = prepare_list(nd->nd_list1, 1, 1);
			if (! node_emptylist(nd->nd_list2)) {
				nd->nd_list2 = prepare_list(nd->nd_list2, 1, 1);
			}
			return;
		case ARROW:
			nd->nd_list2 = prepare_list(nd->nd_list2, 1, 1);
			return;
		case CASE:
			prepare(nd->nd_expr);
			if (nd->nd_flags & ND_HAS_ELSEPART) {
				nd->nd_list2 = prepare_list(nd->nd_list2, 1, 1);
			}
			nd->nd_list1 = prepare_list(nd->nd_list1, 1, 0);
			return;
		}
		prepare(nd->nd_expr);
		nd->nd_list1 = prepare_list(nd->nd_list1, 1, 0);
		nd->nd_list2 = prepare_list(nd->nd_list2, 1, 0);
		break;

	case Call: {
		t_dflst	larg;

		prepare(nd->nd_callee);
		prepare(nd->nd_obj);
		if (nd->nd_symb == FORK) {
			prepare(nd->nd_target);
		}
		node_walklist(nd->nd_parlist, l, e) {
			prepare(e);
		}

		if (nd->nd_symb == '(' && nd->nd_callee->nd_type == std_type) {
			switch(nd->nd_callee->nd_def->df_stdname) {
			case S_ASSERT:
				l = node_getlistel(nd->nd_parlist);
				switch(has_side_effects(l)) {
				case 0:
					break;
				case 1:
					pos_warning(&l->nd_pos, "expression in ASSERT contains a function/operation call");
					mk_simple_expr(l, &new_stats);
					break;
				case 2:
					pos_warning(&l->nd_pos, "expression in ASSERT has side effects");
					mk_simple_expr(l, &new_stats);
					break;
				}
				break;
			case S_DELETENODE:
				if (options['c']) break;
				node_walklist(nd->nd_parlist, l, e) {
					simple_desig(e);
				}
				e = mk_leaf(Check, G_CHECK);
				e->nd_pos = nd->nd_pos;
				e->nd_left = node_copy(nd->nd_parlist);
				e->nd_right = node_copy(node_nextlistel(nd->nd_parlist));
				l = mk_leaf(Stat, CHECK);
				l->nd_pos = e->nd_pos;
				l->nd_expr = e;
				node_enlist(&new_stats, l);
				break;
			case S_STRATEGY:
				if (options['c']) break;
				l = node_nextlistel(node_nextlistel(nd->nd_parlist));
				simple_expr(l);
				e = mk_leaf(Check, CPU_CHECK);
				e->nd_pos = nd->nd_pos;
				e->nd_right = node_copy(l);
				l = mk_leaf(Stat, CHECK);
				l->nd_pos = e->nd_pos;
				l->nd_expr = e;
				node_enlist(&new_stats, l);
				break;
			}
			break;
		}
		if (options['c']) break;

		if (nd->nd_symb == DOLDOL) {
			break;
		}

		larg = param_list_of(nd->nd_callee->nd_type);

		node_walklist(nd->nd_parlist, l, e) {
			t_def	*df = def_getlistel(larg);

			larg = def_nextlistel(larg);

			if (! is_in_param(df) && (e->nd_flags & ND_ALIAS)) {
				aliases(e, l, larg);
			}
		}
		if (nd->nd_symb == FORK && nd->nd_target) {
			simple_expr(nd->nd_target);
			e = mk_leaf(Check, CPU_CHECK);
			e->nd_pos = nd->nd_pos;
			e->nd_right = node_copy(nd->nd_target);
			l = mk_leaf(Stat, CHECK);
			l->nd_pos = e->nd_pos;
			l->nd_expr = e;
			node_enlist(&new_stats, l);
		}
		}
		break;
	}
}

static void
prepare_sets(nd)
	p_node	nd;
{
	/*	Called for binary set operators and assignment statements
		with a binary set operator as rhs. The expression is split
		up into a number of assignment operators.
	*/

	p_node	new = 0;

	if (nd->nd_symb == BECOMES) {
		p_node	rght = nd->nd_expr;

		prepare(nd->nd_desig);
		mk_simple_desig(nd->nd_desig, &new_stats);
		prepare(rght->nd_right);
		nd->nd_expr = rght->nd_right;
		switch(rght->nd_symb) {
		case '+':
			nd->nd_symb = PLUSBECOMES;
			break;
		case '-':
			nd->nd_symb = MINBECOMES;
			break;
		case '*':
			nd->nd_symb = TIMESBECOMES;
			break;
		case '/':
			nd->nd_symb = DIVBECOMES;
			break;
		}
		new = mk_leaf(Stat, BECOMES);
		new->nd_desig = node_copy(nd->nd_desig);
		new->nd_expr = rght->nd_left;
		new->nd_pos = nd->nd_pos;
		prepare(new);
		node_enlist(&new_stats, new);
		free_node(rght);
		return;
	}

	assert(nd->nd_class == Oper);
	if (nd->nd_symb != '-'
	    && nd->nd_right->nd_class == Tmp) {
		/* The only non-commutative set operator is '-'.
		*/
		p_node	tmp = nd->nd_right;
		nd->nd_right = nd->nd_left;
		nd->nd_left = tmp;
	}
	if (nd->nd_left->nd_class != Tmp) {
		mk_temp_expr(nd->nd_left, 0, &new_stats);
	}
	switch(nd->nd_symb) {
	case '-':
		new = mk_leaf(Stat, MINBECOMES);
		break;
	case '+':
		new = mk_leaf(Stat, PLUSBECOMES);
		break;
	case '*':
		new = mk_leaf(Stat, TIMESBECOMES);
		break;
	case '/':
		new = mk_leaf(Stat, DIVBECOMES);
		break;
	}
	new->nd_desig = nd->nd_left;
	new->nd_pos = nd->nd_pos;
	new->nd_expr = nd->nd_right;
	node_enlist(&new_stats, new);
	nd->nd_class = Tmp;
	nd->nd_tmpvar = new->nd_desig->nd_tmpvar;
}

static void
prepare_logic(nd)
	p_node	nd;
{
	p_node	sv;
	p_node	new = nd;
	p_node	new2;

	if (nd->nd_symb == AND || nd->nd_symb == OR) {
		/* First, make these right-associative. */
		if (options['O']) {
			while (nd->nd_symb == nd->nd_left->nd_symb) {
				p_node	l = nd->nd_left;
				nd->nd_left = l->nd_left;
				l->nd_left = l->nd_right;
				l->nd_right = nd->nd_right;
				nd->nd_right = l;
			}
		}
		prepare(nd->nd_left);
		sv = new_stats;
		node_initlist(&new_stats);
		prepare(nd->nd_right);
		if (node_emptylist(new_stats) &&
		    ( ! options['O'] || ! SR_part(nd->nd_right))) {
			new_stats = sv;
			return;
		}
		new = mk_leaf(Stat, ORBECOMES);
		new->nd_pos = nd->nd_pos;
		if (nd->nd_symb == AND) new->nd_symb = ANDBECOMES;
		new->nd_list1 = new_stats;
		new_stats = sv;
		new2 = mk_leaf(Stat, BECOMES);
		new2->nd_pos = new->nd_pos;
		if (nd->nd_right->nd_class == Tmp) {
			new2->nd_desig = node_copy(nd->nd_right);
			new2->nd_expr = nd->nd_left;
			node_enlist(&new_stats, new2);
			new->nd_desig = nd->nd_right;
		}
		else {
			mk_temp_expr(nd->nd_left, 0, &new_stats);
			new->nd_desig = nd->nd_left;
			new2->nd_desig = node_copy(new->nd_desig);
			node_enlist(&(new->nd_list1), new2);
			new2->nd_expr = nd->nd_right;
		}
		node_enlist(&new_stats, new);
		node_enlist(&(new->nd_list1), mk_leaf(Stat, 0));
		nd->nd_class = Tmp;
		nd->nd_symb = IDENT;
		nd->nd_tmpvar = new->nd_desig->nd_tmpvar;
		return;
	}

	mk_simple_desig(nd->nd_desig, &new_stats);
	sv = new_stats;
	node_initlist(&new_stats);
	prepare(nd->nd_expr);
	if (node_emptylist(new_stats) &&
	    ( ! options['O'] || ! SR_part(nd->nd_expr))) {
		new_stats = sv;
		return;
	}
	new = mk_leaf(Stat, BECOMES);
	new->nd_pos = nd->nd_pos;
	new->nd_desig = node_copy(nd->nd_desig);
	new->nd_expr = nd->nd_expr;
	nd->nd_expr = 0;
	node_enlist(&new_stats, new);
	node_enlist(&new_stats, mk_leaf(Stat, 0));
	if (node_emptylist(nd->nd_list1)) {
		nd->nd_list1 = new_stats;
	}
	else {
		p_node	l;
		p_node	n;

		node_walklist(new_stats, l, n) {
			node_fromlist(&new_stats, n);
			node_enlist(&(nd->nd_list1), n);
		}
		node_killlist(&new_stats);
	}
	new_stats = sv;
}

static void
prepare_bats(df)
	t_def	*df;
{
	/*	Handle the bounds-and-tag expressions. They may require
		temporaries as well.
	*/

	p_node	nd;
	t_type	*tp = df->df_type;
	static int
		level;

	level++;
	tp = df->df_type;
	if (! tp->tp_def || tp->tp_def->df_scope == CurrentScope) for (;;) {
	    /* Only for anonymous types and local types. */
	    switch(tp->tp_fund) {
	    case T_UNION:
		if (tp->rec_init) {
			mk_simple_expr(tp->rec_init, &new_stats);
		}
		/* fall through */
	    case T_RECORD:
		walkdefs(tp->rec_scope->sc_def,
			D_FIELD|D_UFIELD|D_OFIELD,
			prepare_bats);
		break;
	    case T_ARRAY: {
		int i;
		for (i = 0; i < tp->arr_ndim; i++) {
			if (tp->arr_bounds(i)) {
				mk_simple_expr(tp->arr_bounds(i)->nd_left, &new_stats);
				mk_simple_expr(tp->arr_bounds(i)->nd_right, &new_stats);
			}
		}
		tp = element_type_of(tp);
		}
		continue;

	    case T_GRAPH:
		walkdefs(tp->gra_root->rec_scope->sc_def,
			D_FIELD,
			prepare_bats);
		walkdefs(tp->gra_node->rec_scope->sc_def,
			D_FIELD,
			prepare_bats);
		break;
	    }
	    break;
	}

	switch(df->df_kind) {
	case D_OFIELD:
		if (df->df_flags & (D_UPPER_BOUND|D_LOWER_BOUND)) break;
	case D_FIELD:
	case D_UFIELD:
		simple_bats(df->fld_bat);
		if (df->df_kind == D_OFIELD
		    && level == 1
		    && (df->df_type->tp_flags & T_INIT_CODE)) {
			nd = mk_leaf(Stat, INIT);
			nd->nd_desig = mk_leaf(Def, IDENT);
			nd->nd_desig->nd_def = df;
			nd->nd_pos = df->df_position;
			node_enlist(&new_stats, nd);
		}
		break;

	case D_VARIABLE:
		if (df->df_flags & D_PART_INDEX) break;
		simple_bats(df->var_bat);

		if ((df->df_flags & D_COPY) ||
		    ((df->df_type->tp_flags & T_INIT_CODE) &&
		     ! is_shared_param(df) &&
		     ! is_in_param(df) &&
		     ( !is_out_param(df) || !(df->df_flags & D_GATHERED)))) {
			nd = mk_leaf(Stat, INIT);
			nd->nd_desig = mk_leaf(Def, IDENT);
			nd->nd_desig->nd_def = df;
			nd->nd_pos = df->df_position;
			node_enlist(&new_stats, nd);
		}
		break;
	}
	level--;
}

static void
simple_bats(bat)
	p_node	bat;
{
	p_node	l;
	p_node	nd;
	node_walklist(bat, l, nd) {
		while (nd->nd_class == Link && nd->nd_symb != UPTO) {
			mk_simple_expr(nd->nd_left->nd_left, &new_stats);
			mk_simple_expr(nd->nd_left->nd_right, &new_stats);
			nd = nd->nd_right;
		}
		if (nd->nd_class == Link) {
			mk_simple_expr(nd->nd_left, &new_stats);
			mk_simple_expr(nd->nd_right, &new_stats);
		}
		mk_simple_expr(nd, &new_stats);
	}
}

static void
simple_desig(nd)
	p_node	nd;
{
	if (! options['O'] || options['Q']) {
		mk_simple_desig(nd, &new_stats);
		return;
	}
	switch(nd->nd_class) {
	case Value:
		if (nd->nd_type != string_type) {
			mk_simple_desig(nd, &new_stats);
		}
		break;
	case Call:
	case Aggr:
	case Row:
	case Oper:
	case Uoper:
		mk_simple_desig(nd, &new_stats);
		break;
	case Ofldsel:
	case Arrsel:
		simple_desig(nd->nd_left);
		simple_expr(nd->nd_right);
		break;
	case Select:
		simple_desig(nd->nd_left);
		break;
	}
}

static void
simple_expr(nd)
	p_node	nd;
{
	if (! options['O'] || options['Q']) {
		mk_simple_expr(nd, &new_stats);
		return;
	}
	switch(nd->nd_class) {
	case Ofldsel:
	case Arrsel:
		simple_desig(nd->nd_left);
		simple_expr(nd->nd_right);
		break;
	case Oper:
		if (nd->nd_left) {
			simple_expr(nd->nd_left);
		}
		/* fall through */
	case Uoper:
		if (nd->nd_right) {
			simple_expr(nd->nd_right);
		}
		break;
	case Call:
		if (suitable_CMSR(nd) == -1) {
			mk_simple_expr(nd, &new_stats);
		}
		break;
	case Select:
		simple_desig(nd->nd_left);
		break;
	}
}

static void
mk_check(nd, kind)
	p_node	nd;
	int	kind;
{
	/*	Produce a check of kind 'kind', and add it to the current
		statement list.
	*/

	p_node	new;

	new = mk_leaf(Check, kind);
	new->nd_pos = nd->nd_pos;
	if (kind == A_CHECK) {
		assert(nd->nd_symb == ARR_INDEX);
		new->nd_left = nd->nd_left;
		nd->nd_left = 0;
		new->nd_dimno = nd->nd_dimno;
	}
	else if (kind != DIV_CHECK && kind != MOD_CHECK) {
		new->nd_left = node_copy(nd->nd_left);
	}
	new->nd_right = node_copy(nd->nd_right);
	nd = mk_leaf(Stat, CHECK);
	nd->nd_pos = new->nd_pos;
	nd->nd_expr = new;
	node_enlist(&new_stats, nd);
}

static void
aliases(arg, alist, flist)
	p_node	arg;
	p_node	alist;
	t_dflst	flist;
{
	p_node	l;
	p_node	nd;
	t_def	*df;
	p_node	new;

	simple_desig(arg);
	node_walklist(alist, l, nd) {
		df = def_getlistel(flist);
		flist = def_nextlistel(flist);
		if (! is_in_param(df)
		    && (nd->nd_flags & ND_ALIAS)
		    && cmp_designators(nd, arg) != DIFFERENT) {
			simple_desig(nd);
			new = mk_leaf(Check, ALIAS_CHK);
			new->nd_pos = arg->nd_pos;
			new->nd_left = node_copy(arg);
			new->nd_right = node_copy(nd);
			nd = mk_leaf(Stat, CHECK);
			nd->nd_pos = new->nd_pos;
			nd->nd_expr = new;
			node_enlist(&new_stats, nd);
		}
	}
}

static int
SR_part(nd)
	p_node	nd;
{
	/* This routine determines whether part of an expression may be
	   used for Code Motion. prepare_logic() needs to know this to
	   determine whether it requires a temporary.
	*/

	p_node	l;
	p_node	n;

	if (! nd) return 0;

	if (suitable_CMSR(nd) > 0) return 1;

	switch(nd->nd_class) {
	case Def:
	case Tmp:
	case Value:
		return 0;
	case Select:
		return SR_part(nd->nd_left);
	case Uoper:
	case Oper:
		if (SR_part(nd->nd_right)) return 1;
		if (SR_part(nd->nd_left)) return 1;
		return 0;
	case Call:
		if (SR_part(nd->nd_callee)) return 1;
		if (SR_part(nd->nd_obj)) return 1;
		if (SR_part(nd->nd_expr)) return 1;
		node_walklist(nd->nd_parlist, l, n) {
			if (SR_part(n)) return 1;
		}
		return 0;
	}
	return 1;
}

static int	side_effect;

static int
has_side_effects(nd)
	p_node	nd;
{
	/* Check if the expression indicated by nd has side effects.
	   In Orca, the only expressions that may have side effects are
	   function/operation calls.
	   For operation calls, we must also check if it is a write operation.
	   Also note that even functions that do not seem to have side effects
	   judging from the interface, may still do I/O.
	*/

	side_effect = 0;
	visit_node(nd, Call, chk_side_effects, 0);
	return side_effect;
}

static int
chk_side_effects(nd)
	p_node	nd;
{
	t_dflst	formals;

	formals = nd->nd_callee->nd_type->prc_params;
	if (nd->nd_symb == '(') {
		/* Function call. First check the function designator.
		   It might be an expression with side effects.
		*/
		visit_node(nd->nd_callee, Call, chk_side_effects, 0);
		if (nd->nd_callee->nd_type == std_type) {
			if (nd->nd_callee->nd_def->df_stdname == S_ADDNODE) {
				/* Side effect on graph. */
				side_effect = 2;
				return 1;
			}
			return 0;
		}
	}
	else {
		/* Check operation: does it write? */
		assert(nd->nd_symb == '$');
		if (nd->nd_callee->nd_def->df_flags & D_HASWRITES) {
			side_effect = 2;
			return 1;
		}

		/* Check object designator: does it have side effects? */
		visit_node(nd->nd_obj, Call, chk_side_effects, 0);
		if (side_effect > 1) return 1;
	}

	/* Check parameters. */

	while (! def_emptylist(formals)) {
		t_def *f = def_getlistel(formals);

		if (! is_in_param(f)) {
			side_effect = 2;
			return 1;
		}
		formals = def_nextlistel(formals);
	}

	side_effect = 1;
	/* Make visit_node visit the actual parameters ... */
	return 0;
}
