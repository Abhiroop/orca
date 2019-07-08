/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/*   S I M P L I F I C A T I O N   A N D   T E M P O R A R I E S   */

/* $Id: simplify.c,v 1.50 1998/02/27 12:03:23 ceriel Exp $ */

/* This file contains simplification routines. The purpose of these routines
   is to simplify expressions, possibly allocating temporaries and adding
   assignments to temporaries, so that we can produce C from the resulting
   parse tree.
*/

#include "ansi.h"
#include "debug.h"

#include <assert.h>
#include <alloc.h>
#include <stdio.h>

#include "simplify.h"
#include "idf.h"
#include "scope.h"
#include "type.h"
#include "temps.h"
#include "oc_stds.h"
#include "options.h"
#include "chk.h"

static p_node	new_stats;

_PROTOTYPE(static p_node simplify_list, (p_node, int));
_PROTOTYPE(static void simplify, (p_node));
_PROTOTYPE(static void simplify_std, (p_node));
_PROTOTYPE(static void simplify_logic, (p_node));
_PROTOTYPE(static void mk_return_expr, (p_node));
_PROTOTYPE(static int must_copy_in_param, (p_node));
#ifdef DEBUG
_PROTOTYPE(static void print_body, (t_def *));
#endif

#define to_designator(nd)	mk_designator(nd, &new_stats)
#define to_simple_desig(nd)	mk_simple_desig(nd, &new_stats)
#define to_simple_expr(nd)	do { \
					simplify(nd); \
					mk_simple_expr(nd, &new_stats); \
				} while (0)

#ifdef DEBUG
static void
print_body(df)
	t_def	*df;
{
	printf("body of %s\n", df->df_idf->id_text);
	if (df->df_kind & (D_OBJECT|D_FUNCTION|D_PROCESS)) {
		dump_nodelist(df->bod_statlist1, 0);
	}
	else {
		printf("read alternatives:\n");
		dump_nodelist(df->bod_statlist1, 0);
		printf("write alternatives:\n");
		dump_nodelist(df->bod_statlist2, 0);
	}
}
#endif

void
simplify_df(df)
	t_def	*df;
{
	/*	Simplify the body of 'df'.
	*/

	t_scope	*sv_scope = CurrentScope;

	assert(df->df_kind & (D_MODULE|D_OBJECT|D_PROCESS|D_FUNCTION|D_OPERATION));

	CurrentScope = df->bod_scope;
	inittemps();
	DO_DEBUG(options['C'], (printf("not simplified:\n"), print_body(df),1));
	df->bod_init = simplify_list(df->bod_init, 1);
	df->bod_statlist1 = simplify_list(df->bod_statlist1, 1);
	df->bod_statlist2 = simplify_list(df->bod_statlist2, 1);
	if (df->df_kind == D_OPERATION && df->opr_dependencies) {
		df->opr_dependencies = simplify_list(df->opr_dependencies, 1);
	}
	DO_DEBUG(options['C'], (printf("simplified:\n"), print_body(df),1));
	CurrentScope = sv_scope;
}

static p_node
simplify_list(l, statlevel)
	p_node	l;
	int	statlevel;
{
	/*	Simplify the p_node 'l', and return the simplified list.
		If 'statlevel' is set, 'l' indicates a statement list,
		and we can add assignments, checks, etc. The global
		variable 'new_stats' is used for this.
	*/

	p_node	ll,
		new_ll,		/* only if statlevel is not set. */
		sv_new_stats = new_stats;
	p_node	nd;

	node_initlist(&new_ll);

	/* Initialize new list. */
	if (statlevel) {
		node_initlist(&new_stats);
	}

	/* Walk through old list, creating new list. */
	node_walklist(l, ll, nd) {
		simplify(nd);
		if (! nd) continue;
		if (statlevel) {
			node_enlist(&new_stats, nd);
		}
		else {
			node_enlist(&new_ll, nd);
		}
	}

	/* Remove old list. */
	node_killlist(&l);

	/* Terminate list. Free temporaries. */
	if (statlevel) {
		l = new_stats;
		new_stats = sv_new_stats;
		return l;
	}
	return new_ll;
}

static void
simplify(arg)
	p_node	arg;
{
	/*	Simplify a node 'arg'. This routine handles the different
		cases.
	*/

	p_node	nd = arg;
	p_node	e;
	p_node	l;

	if (! nd) return;

	switch (nd->nd_class) {
	case Uoper:
		simplify(nd->nd_right);
		break;

	case Link:
		simplify(nd->nd_left);
		simplify(nd->nd_right);
		break;

	case Ofldsel:
		simplify(nd->nd_right);
		break;

	case Arrsel:
		/* Left-hand side as well as right-hand side must be
		   simplified to a simple designator/expression because
		   they may not be evaluated twice.
		*/
		simplify(nd->nd_left);
		if (nd->nd_left->nd_type->tp_fund != T_ARRAY ||
		    ! (nd->nd_left->nd_type->tp_flags & T_CONSTBNDS)) {
			to_simple_desig(nd->nd_left);
		}

		simplify(nd->nd_right);
		/* If this is a graph node selection, we must be able to
		   designate the nodename.
		*/
		if (nd->nd_left->nd_type->tp_fund == T_GRAPH) {
			to_simple_desig(nd->nd_right);
		}
		break;

	case Aggr:
	case Row:
		/* Store aggregate in temporary. */

		nd->nd_memlist = simplify_list(nd->nd_memlist, 0);
		if (nd->nd_type) {
			/* Note that nd->nd_type is not set for individual
			   rows of multidimensional arrays.
			*/
			if (nd->nd_type->tp_fund & (T_BAG|T_SET)) {
				node_walklist(nd->nd_memlist, l, e) {
					to_simple_desig(e);
				}
			}
			mk_temp_expr(nd, 0, &new_stats);
		}
		break;

	case Select:
		/* Graph, record, or union selection. */
		simplify(nd->nd_left);
		simplify(nd->nd_right);
		if (nd->nd_left->nd_type->tp_fund == T_UNION) {
			/* For a union selection, add a check, but only if
			   it is one of the variants, not if it is the
			   union tag.
			*/
			assert(nd->nd_right->nd_class == Def);
			if (! (nd->nd_right->nd_def->df_flags & D_TAG)) {
				to_simple_desig(nd->nd_left);
			}
		}
		break;

	case Oper:
		simplify(nd->nd_left);
		if (nd->nd_symb == AND || nd->nd_symb == OR) {
			simplify_logic(nd);
			break;
		}
		simplify(nd->nd_right);
		if (! (nd->nd_type->tp_fund & (T_SET|T_BAG))) {
			/* We must be able to designate the LHS.
			*/
			if (nd->nd_symb == IN) {
				to_designator(nd->nd_left);
			}
			break;
		}
		break;

	case Call: {
		t_dflst larg;
		int	cnt;
		int	local_operation = 0;

		/* Constructed IN parameters must be converted to
		   designators. If the call is an operation call, all
		   IN parameters must be converted to simple expressions
		   or designators and all OUT parameters must be converted
		   to simple designators.
		   For IN parameters that contain objects, a copy is made.
		   (>>> This is probably not needed for local objects <<<)
		*/
		if (nd->nd_symb == DOLDOL) {
			if (! strcmp(nd->nd_callee->nd_idf->id_text, "distribution")) {
				to_designator(node_getlistel(nd->nd_parlist));
			}
			break;
		}
		simplify(nd->nd_callee);
		if (nd->nd_symb == '$') {
			simplify(nd->nd_obj);
			to_simple_desig(nd->nd_obj);
			if ((nd->nd_callee->nd_def->df_flags & D_NONBLOCKING) &&
			    indicates_local_object(nd->nd_obj)) {
				local_operation = 1;
			}
		}
		else {
			to_designator(nd->nd_callee);
			if (nd->nd_symb == '(') local_operation = 1;
		}
		if (nd->nd_symb == '('
		    && nd->nd_callee->nd_type == std_type) {
			simplify_std(nd);
			break;
		}
		larg = param_list_of(nd->nd_callee->nd_type);
		cnt = 0;
		if (nd->nd_symb == '$') {
			if (nd->nd_type) cnt++;
		}
		node_walklist(nd->nd_parlist, l, nd) {
			t_def *df = def_getlistel(larg);
			simplify(nd);
			if (is_in_param(df)) {
			    if (must_copy_in_param(nd)) {
				mk_temp_expr(nd, 0, &new_stats);
			    }
			    else if (! local_operation) {
				to_simple_desig(nd);
			    }
			    else if (is_constructed_type(nd->nd_type)) {
				to_designator(nd);
			    }
			}
			else {
			    if (! local_operation) to_simple_desig(nd);
			}
			cnt++;
			larg = def_nextlistel(larg);
		}
		nd = arg;
		if (nd->nd_symb == '$' || nd->nd_symb == FORK) {
		    if (CurrentScope->sc_definedby->bod_argtabsz < cnt+dp_flag) {
			CurrentScope->sc_definedby->bod_argtabsz = cnt+dp_flag;
		    }
		}
		if (nd->nd_type
		    && ! nd->nd_target
		    && ((nd->nd_symb == '$' && ! local_operation)
			|| is_constructed_type(nd->nd_type))) {
			mk_temp_expr(nd, 0, &new_stats);
		}
		break;
		}

	case Stat:
		assert(nd != nd->nd_desig);
		if (nd->nd_symb == BECOMES && nd->nd_desig->nd_class != Tmp) {
			switch(cmp_designators(nd->nd_desig, nd->nd_expr)) {
			case SIMILAR:
			case SAME:
				if (! is_constructed_type(nd->nd_desig->nd_type)) {
					break;
				}
				simplify(nd->nd_expr);
				simplify(nd->nd_desig);
				mk_simple_desig(nd->nd_desig, &new_stats);
				mk_simple_desig(nd->nd_expr, &new_stats);
				nd->nd_symb = ALIASBECOMES;
				return;
			}
		}
		simplify(nd->nd_desig);
		switch(nd->nd_symb) {
		case GUARD:
			if (nd->nd_expr->nd_flags & ND_BLOCKS) {
				to_simple_expr(nd->nd_expr);
			}
			break;
		case RETURN:
			if (! nd->nd_expr) break;
			mk_return_expr(nd);
			if (nd->nd_expr->nd_class == Call) {
				nd->nd_expr->nd_target = node_copy(nd->nd_desig);
			}
			break;
		case BECOMES:
			if (nd->nd_desig->nd_type->tp_flags & T_DYNAMIC) {
				to_simple_desig(nd->nd_desig);
			}
			e = nd->nd_expr;
			if (e->nd_class == Call &&
			    (e->nd_symb != '$' ||
			     ! (e->nd_callee->nd_def->df_flags & D_BLOCKING))) {
				/* For blocking operations, nd_target is not
				   set here to prevent assignment to variables
				   in case the operation blocks.
				*/
				e->nd_target = node_copy(nd->nd_desig);
			}
			if (e->nd_class & (Aggr|Row)) {
				e->nd_memlist =
					simplify_list(e->nd_memlist, 0);
				if (nd->nd_desig->nd_type->tp_fund & (T_BAG|T_SET)) {
					p_node	n;
					node_walklist(e->nd_memlist, l, n) {
						to_simple_desig(n);
					}
				}
				to_simple_desig(nd->nd_desig);
				return;
			}
			break;
		case ORBECOMES:
		case ANDBECOMES:
			simplify_logic(nd);
			break;
		case MODBECOMES:
			/* Does not have an equivalent in C. Make sure lhs
			   will only be evaluated once.
			*/
			to_simple_desig(nd->nd_desig);
			break;
		case DIVBECOMES:
			if (nd->nd_expr->nd_type->tp_fund == T_INTEGER) {
				to_simple_desig(nd->nd_desig);
			}
			break;
		case CASE:
			if (nd->nd_list1) {
				p_node	ndx = node_getlistel(nd->nd_list1);

				if (ndx->nd_flags & ND_LARGE_RANGE) {
					to_simple_expr(nd->nd_expr);
				}
			}
			break;
		case ACCESS:
			node_walklist(nd->nd_list1, l, e) {
				to_simple_expr(e);
			}
			return;
		}
		simplify(nd->nd_expr);
		nd->nd_list1 = simplify_list(nd->nd_list1, 1);
		nd->nd_list2 = simplify_list(nd->nd_list2, 1);
		break;
	}
}

static void
simplify_logic(nd)
	p_node	nd;
{
	p_node	sv;
	p_node	new = nd;
	p_node	new2;

	if (nd->nd_symb == AND || nd->nd_symb == OR) {
		sv = new_stats;
		node_initlist(&new_stats);
		simplify(nd->nd_right);
		if (node_emptylist(new_stats)) {
			new_stats = sv;
			return;
		}
		new = mk_leaf(Stat, ORBECOMES);
		new->nd_pos = nd->nd_pos;
		if (nd->nd_symb == AND) new->nd_symb = ANDBECOMES;
		new->nd_list1 = new_stats;
		new_stats = sv;
		mk_temp_expr(nd->nd_left, 0, &new_stats);
		node_enlist(&new_stats, new);
		new->nd_desig = nd->nd_left;
		new2 = mk_leaf(Stat, BECOMES);
		new2->nd_pos = new->nd_pos;
		new2->nd_desig = node_copy(new->nd_desig);
		node_enlist(&(new->nd_list1), new2);
		node_enlist(&(new->nd_list1), mk_leaf(Stat, 0));
		new2->nd_expr = nd->nd_right;
		nd->nd_class = Tmp;
		nd->nd_symb = IDENT;
		nd->nd_tmpvar = new->nd_desig->nd_tmpvar;
		return;
	}

	mk_simple_desig(nd->nd_desig, &new_stats);
	sv = new_stats;
	node_initlist(&new_stats);
	simplify(nd->nd_expr);
	if ( node_emptylist(new_stats)) {
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

void
mk_designator(nd, ndlist)
	p_node	nd;
	p_node	*ndlist;
{
	/*	Convert the expression in 'nd' to a designator,
		allocating temporaries when needed.
	*/

	switch(nd->nd_class) {
	case Value:
		if (nd->nd_type != string_type) {
			mk_temp_expr(nd, 0, ndlist);
		}
		break;
	case Call:
	case Aggr:
	case Row:
	case Oper:
	case Uoper:
		mk_temp_expr(nd, 0, ndlist);
		break;
	}
}

void
mk_simple_desig(nd, ndlist)
	p_node	nd;
	p_node	*ndlist;
{
	/*	Convert any expression to a simple one that can be designated
		(a Select, Tmp, or Def).
	*/

	mk_designator(nd, ndlist);
	switch(nd->nd_class) {
	case Arrsel:
	case Ofldsel:
		mk_temp_expr(nd, 1, ndlist);
		break;
	case Select:
		mk_simple_desig(nd->nd_left, ndlist);
		break;
	}
}

void
mk_simple_expr(nd, ndlist)
	p_node	nd;
	p_node	*ndlist;
{
	/*	Convert any expression to a simple one (a Select, Value,
		Row, Aggr, Def, or Tmp).
	*/

	switch(nd->nd_class) {
	case Arrsel:
	case Ofldsel:
	case Oper:
	case Uoper:
	case Call:
		mk_temp_expr(nd, 0, ndlist);
		break;
	case Select:
		mk_simple_desig(nd->nd_left, ndlist);
		break;
	}
}

static void
simplify_std(arg)
	p_node	arg;
{
	/*	Allocates and initializes temporaries as far as they are
		needed for Orca built-ins.
	*/

	p_node	nd = arg;
	p_node	l;

	assert(nd->nd_callee->nd_class == Def);

	node_walklist(nd->nd_parlist, l, nd) {
		simplify(nd);
	}

	nd = arg;
	switch(nd->nd_callee->nd_def->df_stdname) {
	case S_ADDNODE:
		if (! arg->nd_target) mk_temp_expr(arg, 0, &new_stats);
		if (named_type_of(arg->nd_type)->gra_node->tp_init) {
			to_simple_desig(arg->nd_target);
			to_simple_desig(node_getlistel(arg->nd_parlist));
		}
		break;
	case S_FROM:
		if (! arg->nd_target) mk_temp_expr(arg, 0, &new_stats);
		break;
	case S_INSERT:
	case S_DELETE:
		nd = node_getlistel(arg->nd_parlist);
		simplify(nd);
		to_designator(nd);
		l = node_nextlistel(arg->nd_parlist);
		nd = node_getlistel(l);
		simplify(nd);
		to_designator(nd);
		break;
	}
}

void
mk_temp_expr(arg, isptr, ndlist)
	p_node	arg;
	int	isptr;
	p_node	*ndlist;
{
	/*	Create a temporary for the expression in 'nd'.
		Add its initialization to the node list in 'ndlist'.
	*/

	p_node	nd, new;

	if (arg->nd_class == Tmp) return;

	nd = new_node();
	*nd = *arg;
	nd->nd_next = 0; nd->nd_prev = 0;

	/* An assignment for the temporary. */
	new = mk_leaf(Stat, isptr ? TMPBECOMES : BECOMES);
	new->nd_expr = nd;
	new->nd_pos = nd->nd_pos;
	new->nd_desig = get_tmpvar(nd->nd_type, isptr);
	new->nd_desig->nd_flags |= ND_LHS;
	new->nd_desig->nd_pos = nd->nd_pos;
	if (nd->nd_class == Call) nd->nd_target = node_copy(new->nd_desig);
	node_enlist(ndlist, new);

	arg->nd_class = Tmp;
	arg->nd_symb = IDENT;
	arg->nd_tmpvar = new->nd_desig->nd_tmpvar;
	arg->nd_tmpvar->tmp_expr = node_copy(new->nd_expr);
}

static void
mk_return_expr(nd)
	p_node	nd;
{
	/*	Create an assignment for a RETURN from a function/operation.
	*/

	p_node	new;

	nd->nd_desig = new = mk_leaf(Tmp, IDENT);
	new->nd_type = nd->nd_expr->nd_type;
	new->nd_pos = nd->nd_pos;
	new->nd_flags |= ND_RETVAR|ND_LHS;
}

static int
must_copy_in_param(nd)
	p_node	nd;
{
	if (nd->nd_flags & ND_ALIAS) return 1;
	if ((nd->nd_type->tp_flags & T_HASOBJ)
	    && ! indicates_local_object(nd)) {
		return 1;
	}
	return 0;
}

int
indicates_local_object(nd)
	p_node	nd;
{
	/*	The node indicated by nd indicates a local object if it
		is not a shared parameter of a function or process,
		and, if it is defined in a process, the process has no
		forks.
	*/

	p_node	l;

	if (nd->nd_type->tp_def->df_flags & D_PARTITIONED) return 0;
	switch(CurrentScope->sc_definedby->df_kind) {
	case D_PROCESS:
		l = select_base(nd);
		if (l->nd_class == Tmp && l->nd_tmpvar->tmp_ispointer) {
			l = select_base(l->nd_tmpvar->tmp_expr);
		}
		if (l->nd_class != Def) {
			if (CurrentScope->sc_definedby->df_flags & D_HASFORKS) {
				return 0;
			}
		}
		else if	(l->nd_def->df_flags & D_SHAREDOBJ) {
			return 0;
		}
		/* Fall through */
	case D_FUNCTION:
		l = select_base(nd);
		if (l->nd_class == Tmp && l->nd_tmpvar->tmp_ispointer) {
			l = select_base(l->nd_tmpvar->tmp_expr);
		}
		if (l->nd_class != Def ||
		    ! is_shared_param(l->nd_def)) {
			return 1;
		}
		return 0;
	}
	return 1;
}
