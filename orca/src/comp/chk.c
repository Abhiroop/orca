/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/*  E X P R E S S I O N	   C H E C K E R  */

/* $Id: chk.c,v 1.60 1998/03/06 15:46:11 ceriel Exp $ */

#include "debug.h"
#include "ansi.h"

#include <stdio.h>
#include <assert.h>
#include <alloc.h>

#include "chk.h"
#include "LLlex.h"
#include "scope.h"
#include "misc.h"
#include "main.h"
#include "oc_stds.h"
#include "error.h"
#include "const.h"
#include "temps.h"
#include "node.h"
#include "options.h"
#include "specfile.h"
#include "f_info.h"
#include "visit.h"

_PROTOTYPE(static int allowed_types, (int));
_PROTOTYPE(static void df_error, (char *, t_def *, t_pos *));
_PROTOTYPE(static p_node chk_call, (p_node, p_node));
_PROTOTYPE(static p_node chk_operation, (p_node, t_idf *, p_node));
_PROTOTYPE(static p_node getarg, (p_node *, int, int, t_def *));
_PROTOTYPE(static p_node getname, (p_node *, int, int, t_def *));
_PROTOTYPE(static void chk_params, (t_dflst, p_node, t_def *));
_PROTOTYPE(static int cmp_nodes, (p_node, p_node));
_PROTOTYPE(static int chk_ofld_expr, (p_node));
_PROTOTYPE(static p_node prevnode, (p_node, p_node));
_PROTOTYPE(static void chk_assignment_allowed, (p_node));
_PROTOTYPE(static int is_assign_operator, (int));
_PROTOTYPE(static void chk_bool, (p_node));
_PROTOTYPE(static void chk_is_ass_desig, (p_node));
_PROTOTYPE(static p_node down_not_oper, (p_node));
_PROTOTYPE(static int chk_indices, (p_node, t_type *, char *, char *));
_PROTOTYPE(static p_node flatten_index, (p_node, int, t_pos *));
_PROTOTYPE(static int parameter_or_object, (p_node));

static int \
	in_dependency_section;

int	in_doldol = 0;		/* Set when checking arguments in a $$
				   call. Used to accept operation names as an
				   argument.
				*/

static struct loop_list {
	p_node	loop;
	struct loop_list
		*enclosing;
}	*looplist;

static int
	forlooplevel;

p_node
chk_exit()
{
	/*	Exit statement. Check that it resides in a loop, and mark
		the loop as containing an EXIT or RETURN.
	*/
	p_node	nd = mk_leaf(Stat, EXIT);

	if (! looplist) {
		error ("EXIT not in a loop");
	}
	else {
		looplist->loop->nd_flags |= ND_EXIT_OR_RET;
	}
	return nd;
}

void
start_dependency_section(df)
	t_def	*df;
{
	if (! (df->df_flags & D_PARALLEL)) {
		error("DEPENDENCIES section not allowed in a sequential operation");
	}
	in_dependency_section = 1;
}

static int uses_parameter_or_object;

static int
parameter_or_object(nd)
	p_node	nd;
{
	assert(nd->nd_class & (Def|Ofldsel));

	if (nd->nd_class == Ofldsel ||
	    nd->nd_def->df_kind == D_OFIELD ||
	    is_parameter(nd->nd_def)) {
		uses_parameter_or_object = 1;
	}

	return 0;
}

void
end_dependency_section(df, nd)
	t_def	*df;
	p_node	nd;
{
	/*	This routine checks if the dependencies supplied by the user
		are static or dynamic. The only way in which they can be
		dynamic is that they use an object field or a parameter.
	*/
	df->opr_dependencies = nd;
	df->df_flags |= D_HAS_DEPENDENCIES;
	uses_parameter_or_object = 0;
	visit_ndlist(nd, Def|Ofldsel, parameter_or_object, 0);
	if (! uses_parameter_or_object) df->df_flags |= D_SIMPLE_DEPENDENCIES;
	in_dependency_section = 0;
}

p_node
start_loop(sym)
	int	sym;
{
	p_node	nd = mk_leaf(Stat, sym);
	struct loop_list
		*loop = (struct loop_list *) Malloc(sizeof(struct loop_list));

	assert(sym == DO || sym == REPEAT || sym == FOR);
	loop->loop = nd;
	loop->enclosing = looplist;
	looplist = loop;
	if (sym == FOR) forlooplevel++;
	return nd;
}

p_node
end_loop(nd, expr, list)
	p_node	nd;
	p_node	expr;
	p_node	list;
{
	p_node	update = 0;
	struct loop_list
		*loop = looplist;

	if (expr) {
		/* If it is a WHILE or REPEAT loop, create a conditional exit
		   statement.
		*/
		update = mk_leaf(Stat, COND_EXIT);
		update->nd_expr = expr;
		update->nd_pos = expr->nd_pos;
		chk_bool(expr);
	}

	nd->nd_list1 = list;

	switch(nd->nd_symb) {
	case REPEAT:
		nd->nd_symb = DO;
		/* insert the conditional exit statement at the end of the
		   list.
		*/
		node_enlist(&nd->nd_list1, update);
		break;

	case DO:
		if (expr) {
			/* If optimizing, build the WHILE statement as
				IF expr THEN
				DO
					body
					IF NOT expr EXIT;
				OD
			   Otherwise, build it as
				DO
					IF NOT expr EXIT;
					body
				OD
			*/
			p_node	x;

			update->nd_expr = mk_leaf(Uoper, NOT);
			update->nd_expr->nd_pos = expr->nd_pos;
			update->nd_expr->nd_type = expr->nd_type;
			if (options['O']) {
				node_enlist(&nd->nd_list1, update);
				x = mk_leaf(Stat, IF);
				x->nd_pos = expr->nd_pos;
				x->nd_expr = expr;
				node_enlist(&(x->nd_list1), nd);
				nd = x;
				update->nd_expr->nd_right = node_copy(expr);
			}
			else {
				node_insert(&nd->nd_list1, update);
				nd->nd_list1 = update;
				update->nd_expr->nd_right = expr;
			}
			update->nd_expr = down_not_oper(update->nd_expr);
		}
		break;
	case FOR:
		forlooplevel--;
		/* Create an update-statement for the for-loop variable. */
		update = mk_leaf(Stat, FOR_UPDATE);
		update->nd_expr = nd->nd_expr;
		update->nd_desig = nd->nd_desig;
		node_enlist(&nd->nd_list1, update);
		close_scope();
		if (nd->nd_expr->nd_flags & ND_FORDONE) {
			nd->nd_flags |= ND_FORDONE;
		}
		break;
	}
	looplist = loop->enclosing;
	free(loop);
	return nd;
}

p_node
chk_ifstat(expr, list)
	p_node	expr;
	p_node	list;
{
	p_node	nd = mk_leaf(Stat, IF);

	nd->nd_expr = expr;
	nd->nd_list1 = list;
	node_initlist(&nd->nd_list2);
	chk_bool(expr);
	return nd;
}

p_node
chk_guard(expr, list)
	p_node	expr;
	p_node	list;
{
	p_node	nd = mk_leaf(Stat, GUARD);

	nd->nd_expr = expr;
	nd->nd_list1 = list;
	chk_bool(expr);
	return nd;
}

p_node
chk_elsifpart(nd, expr, list)
	p_node	nd;
	p_node	expr;
	p_node	list;
{
	/* Make the ELSIF part look like a nested IF. Note that a possible
	   ELSE or ELSIF part later on belongs to this IF, not to the original
	   one. The place-holder for the new IF is returned, but this one should
	   only be used to connect later parts.
	*/
	p_node	n = mk_leaf(Stat, IF);

	node_enlist(&nd->nd_list2, n);
	n->nd_expr = expr;
	chk_bool(expr);
	n->nd_list1 = list;
	node_initlist(&n->nd_list2);
	return n;
}

void
add_elsepart(nd, list)
	p_node	nd;
	p_node	list;
{
	nd->nd_list2 = list;
}

p_node
chk_forloopheader(id, expr, rght)
	t_idf	*id;
	p_node	expr;
	p_node	rght;
{
	/*	The identifier indicated by 'id' is used as a FOR-loop
		control variable. Check that it does not hide any other
		declarations of the same name.
		Also check the FOR-loop expression, and declare the
		FOR-loop variable.
	*/

	t_scope	*sc = CurrentScope;
	t_type	*tp;
	t_def	*df;
	p_node	nd = start_loop(FOR);

	for(;;) {
		/* Look for a variable with the same name in enclosing scopes.
		   Earlier versions of Orca did not have FOR-loop variables
		   declared automagically, so warn users about variables
		   with the same name.
		*/
		if (lookup(id, sc, 0)) {
			warning("FOR-loop variable \"%s\" hides earlier declaration", id->id_text);
			break;
		}
		if (sc == ProcScope) break;
		sc = enclosing(sc);
	}

	tp = expr->nd_type;
	if (rght) {
		/* Loop over a range. */
		expr = chk_upto(expr, rght, tp);
	}
	else {
		/* Loop over a set or bag. */
		mark_defs(expr, D_USED);
		if (tp != error_type &&
		    !(tp->tp_fund & (T_BAG|T_SET))) {
			pos_error(&expr->nd_pos, "FOR-loop expression must be a range, set, or bag");
			tp = error_type;
		}
		else tp = element_type_of(tp);
	}
	nd->nd_expr = expr;

	/* Open a scope in which the loop-variable lives. */
	open_scope(OPENSCOPE);
	nd->nd_desig = mk_leaf(Def, IDENT);
	nd->nd_desig->nd_def = df = define(id, CurrentScope, D_VARIABLE);
	df->df_flags |= D_DEFINED|D_USED|D_FORLOOP;
	CurrentScope->sc_FOR = ProcScope->sc_FOR;
	ProcScope->sc_FOR = CurrentScope;
	df->df_type = tp;
	df->var_level = forlooplevel;
	return nd;
}

p_node
chk_access(list)
	p_node	list;
{
	p_node	nd = mk_leaf(Stat, ACCESS);

	nd->nd_list1 = list;
	if (! in_dependency_section) {
		error("ACCESS statement outside DEPENDENCY section");
	}
	else {
		chk_indices(list, CurrDef->df_type, "index", "indices");
	}
	return nd;
}

void
mark_defs(nd, flags)
	p_node	nd;
	int	flags;
{
	/*	D_USED|D_DEFINED marking. The designator indicated by 'nd' is
		used or defined, depending on the 'flags' value.
		Propagate this fact down the 'nd' tree, marking any definitions
		found along the way.
	*/

	p_node	l;

	if (! nd) return;

	switch(nd->nd_class) {
	case Arrsel:
		mark_defs(nd->nd_left, flags);
		mark_defs(nd->nd_right, D_USED);    /* index is only used. */
		break;

	case Aggr:
	case Row:
		node_walklist(nd->nd_memlist, l, nd) {
			mark_defs(nd, flags);
		}
		break;

	case Uoper:
		mark_defs(nd->nd_right, flags);
		break;

	case Call:
		mark_defs(nd->nd_callee, flags);
		mark_defs(nd->nd_obj, flags);
		break;

	case Def:
		nd->nd_def->df_flags |= flags;
		if (nd->nd_def->df_kind & D_IMPORTED) {
			nd->nd_def->imp_def->df_flags |= flags;
		}
		break;

	case Select:
	case Oper:
	case Link:
		mark_defs(nd->nd_left, flags);
		if (nd->nd_right) mark_defs(nd->nd_right, flags);
		break;
	}
}

static void
chk_bool(exp)
	p_node	exp;
{
	/*	Check that the expression indicated by 'exp' is a
		boolean expression.
	*/

	if (exp->nd_type != error_type
	    && exp->nd_type != bool_type) {
		pos_error(&exp->nd_pos, "boolean expression expected");
	}
	mark_defs(exp, D_USED);
}

p_node
chk_is_const_expression(exp)
	p_node	exp;
{
	if (! (exp->nd_flags & ND_CONST)) {
		error("constant expression expected");
		/* make sure that it looks constant ... */
		kill_node(exp);
		exp = mk_leaf(Value, INTEGER);
		exp->nd_type = error_type;
	}
	return exp;
}

p_node
chk_selection(nd, id)
	p_node	nd;
	t_idf	*id;
{
	/*	Check that 'id' is a valid selection of the expression node
		indicated by 'nd'.
	*/

	t_type	*tp = nd->nd_type;
	t_def	*df;
	t_scope	*sc;

	if (nd->nd_type == error_type) return nd;

	switch(nd->nd_class) {
	case Def:
		df = nd->nd_def;
		if (df->df_kind & (D_OBJECT|D_MODULE)) {
			/* A selection from an object or module.
			   Look for the identifier in the object's or module's
			   scope. Check that it is exported.
			*/
			t_def *df1 = lookup(id, df->bod_scope, 0);

			mark_defs(nd, D_USED);
			if (! df1) {
				id_not_declared(id);
				df = define(id, df->bod_scope, D_ERROR);
			}
			else if (! (df1->df_flags & D_EXPORTED)) {
				id_not_declared(id);
				df = df1;
			}
			else df = df1;
			nd->nd_def = df;
			nd->nd_type = df->df_type;
			break;
		}
		/* If not from an object or module, it muet be a variable,
		   constant, or object field.
		*/
		if (! (df->df_kind & (D_CONST|D_VARIABLE|D_OFIELD|D_ERROR))) {
			pos_error(&nd->nd_pos, "illegal selection");
			nd->nd_type = error_type;
			break;
		}
		nd = chk_designator(nd);
		/* Fall through */
	case Arrsel:
	case Select:
	case Ofldsel:
		switch(tp->tp_fund) {
		case T_RECORD:
		case T_UNION:
			sc = tp->rec_scope;
			break;
		case T_GRAPH:
			sc = tp->gra_root->rec_scope;
			break;
		case T_OBJECT:
			sc = ProcScope;
			while (sc && sc->sc_definedby
			       && sc->sc_definedby->df_kind != D_OBJECT) {
				sc = enclosing(sc);
			}
			if (sc && sc->sc_definedby) break;
			/* Fall through */
		default:
			pos_error(&nd->nd_pos, "illegal selection");
			nd->nd_type = error_type;
			return nd;
		}

		/* Lookup the identifier in the scope indicated by the
		   left-hand side.
		*/
		df = lookup(id, sc, 0);
		if (! df) {
			id_not_declared(id);
			df = define(id, sc, D_ERROR);
			nd->nd_type = error_type;
			break;
		}

		/* If the selection is an object field, this is only allowed
		   within an operation.
		*/
		if (df->df_kind == D_OFIELD) {
			sc = ProcScope;
			if (! sc || ! sc->sc_definedby ||
			    sc->sc_definedby->df_kind != D_OPERATION ||
			    in_dependency_section) {
				pos_error(&nd->nd_pos, "illegal object field selection outside operation");
			}
		}

		/* If the left-hand side indicates a constant, the selection
		   can be evaluated at compile time.
		*/
		if (const_available(nd)) {
			p_node	l = nd->nd_def->con_const->nd_memlist;
			t_def	*df1 = sc->sc_def;

			if (tp->tp_fund == T_RECORD) {
				while (df1 != df) {
					l = node_nextlistel(l);
					df1 = df1->df_nextinscope;
				}
			}
			else {
			    assert(tp->tp_fund == T_UNION);
			    /* For a selection from a constant union, the
			       selection identifier must correspond with
			       the tag-field value in the union.
			    */
			    if (! (df->df_flags & D_TAG)) {
				p_node	exp = node_getlistel(l);

				assert(exp->nd_class == Value);
				if (df->fld_tagvalue->nd_int != exp->nd_int) {
					pos_error(&nd->nd_pos,
						  "illegal union selection");
				}
				l = node_nextlistel(l);
			    }
			}
			nd = new_node();
			*nd = *node_getlistel(l);
			break;
		}

		/* Create an expression node for the selection. */
		nd = mk_expr(Select, '.', nd, mk_leaf(Def, IDENT));
		nd->nd_right->nd_def = df;
		nd->nd_type = df->df_type;
		nd->nd_right->nd_type = nd->nd_type;
		break;
	default:
		crash("chk_selection");
	}

	return nd;
}

p_node
chk_graphrootsel(nd, id)
	p_node	nd;
	t_idf	*id;
{
	/*	Check that 'id' is a valid ! selection of a graph root:
		It must be a field of the root of the graph, it must be a
		nodename of this graph type.
		(Note: g!x is a short-hand for g[g.x]).
	*/

	t_type	*tp = nd->nd_type;
	t_def	*df;
	t_scope	*sc;

	if (tp == error_type) return nd;
	if (tp->tp_fund != T_GRAPH) {
		pos_error(&nd->nd_pos, "! only allowed on graph type");
		nd->nd_type = error_type;
		return nd;
	}
	sc = tp->gra_root->rec_scope;
	df = lookup(id, sc, 0);
	if (! df) {
		id_not_declared(id);
		df = define(id, sc, D_ERROR);
		nd->nd_type = error_type;
		return nd;
	}
	if (df->df_type->tp_fund != T_NODENAME
	    || named_type_of(df->df_type) != tp) {
		pos_error(&nd->nd_pos,
			  "incompatible graph-node selector type");
	}
	nd = mk_expr(Select, '!', nd, mk_leaf(Def, IDENT));
	nd->nd_right->nd_def = df;
	nd->nd_right->nd_type = df->df_type;
	nd->nd_type = tp->gra_node;

	return nd;
}

static int
chk_indices(l, tp, s, plural)
	p_node	l;
	t_type	*tp;
	char	*s;
	char	*plural;
{
	/* Check indices for an array or partitioned object field access.
	   Return 1 if all indices are constant.
	*/
	int	i;
	p_node	ip;
	p_node	ind;
	int	retval = 1;

	i = 0;
	node_walklist(l, ip, ind) {
		if (i >= tp->arr_ndim) {
			pos_error(&ind->nd_pos, "too many %s", plural);
			break;
		}
		/* Index must be assignment compatible ??? */

		mark_defs(ind, D_USED);

		if (! tst_ass_compat(tp->arr_index(i), ind->nd_type)) {
			pos_error(&ind->nd_pos, "incompatible %s type", s);
		}
		if (! const_available(ind)) retval = 0;
		i++;
	}
	if (i < tp->arr_ndim) {
		pos_error(&l->nd_pos, "too few %s", plural);
	}
	return retval;
}

p_node
chk_arrayselection(lhs, index_list)
	p_node	lhs;
	p_node	index_list;
{
	/*	Check a node that looks like an array selection. It could be
		either that or a graph node.
	*/

	t_type	*ltp;
	p_node	ip;
	p_node	nd;
	p_node	ind;
	int	i,
		const_index = 1;

	lhs = chk_designator(lhs);
	ltp = lhs->nd_type;

	nd = mk_leaf(Arrsel, '[');
	nd->nd_left = lhs;
	nd->nd_type = error_type;

	if (ltp == error_type) {
		node_killlist(&index_list);
		return nd;
	}

	switch(lhs->nd_class) {
	case Def:
	    {
		/* Process partitioned-object field indices.
		   If the current object is a partitioned object, and
		   the lhs is an object field, and it is not a lower or
		   an upper bound, and the object field is not indexed yet,
		   the current expression is an indexed object field.
		*/
		t_def	*df = lhs->nd_def;
		int	correct;

		if (! (CurrDef->df_flags & D_PARTITIONED)) break;

		if (df->df_kind != D_OFIELD ||
		    (df->df_flags&(D_LOWER_BOUND|D_UPPER_BOUND)) ||
		    (lhs->nd_flags & ND_OFLD_SELECTED)) {
			break;
		}

		chk_indices(index_list, CurrDef->df_type, "index", "indices");
		nd->nd_type = lhs->nd_type;
		nd->nd_class = Ofldsel;

		ip = index_list;
		ind = 0;
		correct = 0;
		for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
			p_node	n = mk_leaf(Oper, ARR_INDEX);
			if (! ind) ind = n;
			else ind->nd_right = n;
			n->nd_right = node_getlistel(ip);
			if (! n->nd_right) break;
			mark_defs(n->nd_right, D_USED);

			/* Try to detect whether the indices are identical
			   to the operation parameters. If they are, the
			   indexing can be removed.
			*/
			if (n->nd_right->nd_class == Def &&
			    (n->nd_right->nd_def->df_flags & D_PART_INDEX) &&
			    n->nd_right->nd_def->var_level == i) {
				correct++;
			}
			else {
				(void) chk_ofld_expr(n->nd_right);
			}
			n->nd_dimno = i;
			n->nd_type = int_type;
			n->nd_pos = n->nd_right->nd_pos;
			ip = node_nextlistel(ip);
			if (ip) {
				/* More than one dimension. Flatten.
				*/
				ind = flatten_index(ind, i, &n->nd_pos);
			}
		}

		nd->nd_right = ind;

		if (correct == CurrDef->df_type->arr_ndim) {
			/* All indeces correspond to the operation parameters.
			   So, just remove all indexing.
			*/
			nd->nd_left = 0;
			kill_node(nd);
			lhs->nd_flags |= ND_OFLD_SELECTED;
			node_killlist(&index_list);
			return lhs;
		}

		if (ProcScope && ProcScope->sc_definedby) {
			ProcScope->sc_definedby->df_flags |= D_HAS_OFLDSEL;
		}
		node_killlist(&index_list);
		return nd;
	    }
	}
	switch(ltp->tp_fund) {
	case T_ARRAY:
		nd->nd_type = element_type_of(ltp);

		const_index = chk_indices(index_list, ltp, "index", "indices");
		ip = index_list;
		ind = 0;
		for (i = 0; i < ltp->arr_ndim; i++) {
			p_node	n = mk_leaf(Oper, ARR_INDEX);
			if (! ind) ind = n;
			else ind->nd_right = n;
			n->nd_right = node_getlistel(ip);
			if (n->nd_right == NULL) break;
			if ((ltp->tp_flags & T_CONSTBNDS) &&
			    ltp->arr_bounds(i)->nd_left->nd_int != 0) {
				n->nd_right = mk_expr(Oper, '-', n->nd_right, node_copy(ltp->arr_bounds(i)->nd_left));
				n->nd_right->nd_type = int_type;
				n->nd_right->nd_pos = n->nd_pos;
			}
			n->nd_dimno = i;
			n->nd_type = int_type;
			n->nd_pos = n->nd_right->nd_pos;
			ip = node_nextlistel(ip);
			if (ip) {
				/* More than one dimension. Flatten.
				*/
				ind = flatten_index(ind, i, &n->nd_pos);
			}
		}

		nd->nd_right = ind;

		if (const_available(lhs) && const_index) {
			/* Constant index, constant array, so we can get the
			   value at compile time.
			*/
			p_node	n;
			p_node	a_el;

			if (ltp == string_type) {
				i = node_getlistel(index_list)->nd_int;
				assert(lhs->nd_class == Value);
				if (i <= 0 || i > lhs->nd_slen) {
				pos_error(&nd->nd_pos,
						"array bound error in constant expression");
					break;
				}
				i = lhs->nd_string[i-1];
				n = nd;
				n->nd_left = 0;
				nd = mk_leaf(Value, CHARACTER);
				nd->nd_type = char_type;
				nd->nd_int = i;
				nd->nd_pos = n->nd_pos;
				kill_node(n);
				node_killlist(&index_list);
				return nd;
			}

			n = nd->nd_left;
			assert(n->nd_class == Def);
			n = n->nd_def->con_const;
			assert(n->nd_class & (Aggr|Row));
			a_el = n->nd_memlist;
			node_walklist(index_list, ip, n) {
				/* Compute offset. If the type specifies the
				   bounds, the lower bound is there as well.
				   Otherwise the lower bound is 1, unless
				   the index type is an enumeration.
				*/
				i = n->nd_int;
				if (ltp->tp_flags & T_CONSTBNDS) {
				    i -= ltp->arr_bounds(i)->nd_left->nd_int;
				}
				else if (n->nd_type->tp_fund != T_ENUM) i--;

				while (i > 0 && ! node_emptylist(a_el)) {
					i--;
					a_el = node_nextlistel(a_el);
				}
				if (node_emptylist(a_el) || i < 0) {
					pos_error(&n->nd_pos,
						"array bound error in constant expression");
					node_killlist(&index_list);
					return nd;
				}
				if (! node_emptylist(ip)) {
					a_el =
					 node_getlistel(a_el)->nd_memlist;
				}
			}
			kill_node(nd);
			nd = new_node();
			*nd = *node_getlistel(a_el);
		}
		break;
	case T_GRAPH:
		nd->nd_type = ltp->gra_node;
		ind = node_getlistel(index_list);
		nd->nd_right = ind;
		if (node_nextlistel(index_list)) {
			pos_error(&nd->nd_pos, "more than one index in graph selection");
		}
		if (ind->nd_type->tp_fund != T_NODENAME
		    || named_type_of(ind->nd_type) != ltp) {
			pos_error(&ind->nd_pos,
				  "incompatible graph-node selector type");
		}
		break;
	default:
		pos_error(&nd->nd_pos, "[ only allowed on graphs and arrays");
		break;
	}
	node_killlist(&index_list);
	return nd;
}

static p_node
flatten_index(ind, dim, pos)
	p_node	ind;
	int	dim;
	t_pos	*pos;
{
	/* Flatten index computation for multidimensional arrays.
	   Multiply index computed so far with the number of elements in
	   the next dimension, and add the next index.
	*/
	ind = mk_expr(Oper, '*', ind, (p_node) 0);
	ind->nd_type = int_type;
	ind->nd_pos = *pos;
	ind->nd_right = mk_leaf(Uoper, ARR_SIZE);
	ind->nd_right->nd_dimno = dim+1;
	ind->nd_right->nd_type = int_type;
	ind = mk_expr(Oper, '+', ind, (p_node) 0);
	ind->nd_type = int_type;
	ind->nd_pos = *pos;
	return ind;
}

static int
chk_ofld_expr(nd)
	p_node	nd;
{
	/*	Check a partitioned object field index expression, to see if
		if it can be used to generate a static PDG-generation function.
	*/
	static int
		level = 0;
	int	retval = 1;
	t_def	*df;

	level++;
	switch(nd->nd_class) {
	case Value:
		retval = 0;
		break;
	case Select:
		retval = chk_ofld_expr(nd->nd_left);
		break;
	case Arrsel:
		retval = chk_ofld_expr(nd->nd_right) +
			 chk_ofld_expr(nd->nd_left);
		break;
	case Oper:
		switch(nd->nd_symb) {
		case IN:
			break;
		default:
			retval = 0;
			if (nd->nd_left) retval += chk_ofld_expr(nd->nd_left);
			if (nd->nd_right) retval += chk_ofld_expr(nd->nd_right);
			break;
		}
		break;
	case Uoper:
		retval = 0;
		if (nd->nd_right) retval += chk_ofld_expr(nd->nd_right);
		break;
	case Def:
		df = nd->nd_def;
		if (df->df_kind == D_OFIELD) {
			if (df->df_flags & (D_UPPER_BOUND|D_LOWER_BOUND)) {
				retval = 0;
			}
		}
		else if (df->df_flags & (D_PART_INDEX|D_DATA)) {
			retval = 0;
		}
		break;
	}
	level--;
	if (level == 0) {
		if (retval >= 1 && ProcScope && ProcScope->sc_definedby) {
		    ProcScope->sc_definedby->df_flags |= D_HAS_COMPLEX_OFLDSEL;
		}
	}
	return retval;
}

static int
allowed_types(operator)
	int	operator;
{
	/*	Determines for each operator the allowed base types, and
		returns a bit-mask of these bases.
	*/

	switch(operator) {
	case '+':
	case '-':
	case '*':
	case '/':
	case PLUSBECOMES:
	case MINBECOMES:
	case TIMESBECOMES:
	case DIVBECOMES:
		return T_REAL|T_INTEGER|T_SET|T_BAG|T_NUMERIC;
	case '(':
		return 0xffff;
	case '%':
	case MODBECOMES:
	case '|':
	case B_ORBECOMES:
	case '&':
	case B_ANDBECOMES:
	case '^':
	case B_XORBECOMES:
	case LEFTSHIFT:
	case LSHBECOMES:
	case RIGHTSHIFT:
	case RSHBECOMES:
	case '~':
		return T_INTEGER;
	case OR:
	case AND:
	case ORBECOMES:
	case ANDBECOMES:
	case NOT:
		return T_ENUM;
	case '=':
	case NOTEQUAL:
		return	T_NODENAME|T_SET|T_REAL|T_INTEGER|T_ENUM|
			T_RECORD|T_BAG|T_ARRAY|T_UNION|T_NUMERIC|T_SCALAR;
	case GREATEREQUAL:
	case LESSEQUAL:
	case '<':
	case '>':
		return T_REAL|T_INTEGER|T_ENUM|T_SCALAR|T_NUMERIC;

	default:
		crash("allowed_types");
	}
	/*NOTREACHED*/
	return 0;
}

p_node
chk_relational(left, right, oper)
	p_node	left;
	p_node	right;
	int	oper;
{
	/*	Check a relational expression. This routine handles the
		different operators.
	*/

	int	ok = 0;
	p_node	nd = mk_expr(Oper, oper, left, right);

	nd->nd_type = bool_type;
	if (left->nd_type == error_type
	    || right->nd_type == error_type) return nd;

	switch(oper) {
	case IN:
		if (!(right->nd_type->tp_fund & (T_SET | T_BAG))) {
			pos_error(&right->nd_pos,
				"right operand of \"IN\" must be a set or bag");
			break;
		}
		if (! tst_ass_compat(element_type_of(right->nd_type), left->nd_type)) {
			pos_error(&nd->nd_pos,
				"type incompatibility in \"in\"");
			break;
		}
		ok = 1;
		break;
	case '=':
	case NOTEQUAL:
		if (! tst_equality_allowed(left->nd_type)) {
			pos_error(&nd->nd_pos,
				  "illegal operand type(s) in %s",
				  symbol2str(nd->nd_symb));
			break;
		}
		/* Fall through */
	case GREATEREQUAL:
	case LESSEQUAL:
	case '<':
	case '>':
		if (! (allowed_types(nd->nd_symb) & left->nd_type->tp_fund)) {
			pos_error(&nd->nd_pos,
				  "illegal operand type(s) in %s",
				  symbol2str(nd->nd_symb));
			break;
		}
		if (!tst_compat(left->nd_type, right->nd_type)) {
			pos_error(&nd->nd_pos,
				  "%s in operands of %s",
				  incompat(left->nd_type, right->nd_type),
				  symbol2str(nd->nd_symb));
			break;
		}
		ok = 1;
		break;
	default:
		crash("chk_relational");
	}

	if (ok
	    && left->nd_class == Value
	    && right->nd_class == Value) {
		/* Constant expression. Evaluate now. */
		if (left->nd_type->tp_fund == T_REAL) {
			return fcst_relational(nd);
		}
		return cst_relational(nd);
	}
	return nd;
}

p_node
chk_unary(nd, oper)
	p_node	nd;
	int	oper;
{
	/*	Check an unary operator. Perform it if the operand is a
		constant.
	*/
	t_type	*tp = nd->nd_type;

	nd = mk_expr(Uoper, oper, (p_node) 0, nd);
	nd->nd_type = tp;
	if (tp == error_type) return nd;
	if ((oper == NOT && tp != bool_type)
	    || ! (allowed_types(oper) & tp->tp_fund)) {
		pos_error(&nd->nd_pos,
			  "illegal operand type in unary %s",
			  symbol2str(oper));
		return nd;
	}
	if (oper == '(' || oper == '+') {
		return cst_unary(nd);
	}
	if (nd->nd_right->nd_class == Value) {
		if (nd->nd_right->nd_type->tp_fund == T_REAL) {
			return fcst_unary(nd);
		}
		return cst_unary(nd);
	}
	if (oper == NOT) return down_not_oper(nd);
	return nd;
}

static p_node
down_not_oper(nd)
	p_node	nd;
{
	/*	Push the NOT operator as far down in the expression tree as
		possible. Use the following rules:

			NOT (A AND B) == (NOT A) OR (NOT B)
			NOT (A OR B) == (NOT A) AND (NOT B)
			NOT(NOT(A)) == A
			NOT (A = B) == A NOTEQUAL B
			NOT (A < B) == A >= B, et cetera.

		The purpose of this is to improve the translation of COND_EXIT
		and IF.
	*/

	p_node	n = nd->nd_right;
	p_node	new;

	assert(nd->nd_symb == NOT);

	switch(n->nd_symb) {
	case AND:
	case OR:
		new = mk_expr(Uoper, NOT, (p_node) 0, n->nd_left);
		new->nd_pos = n->nd_left->nd_pos;
		new->nd_type = n->nd_left->nd_type;
		n->nd_left = down_not_oper(new);
		new = mk_expr(Uoper, NOT, (p_node) 0, n->nd_right);
		new->nd_pos = n->nd_right->nd_pos;
		new->nd_type = n->nd_right->nd_type;
		n->nd_right = down_not_oper(new);
		n->nd_symb = n->nd_symb == AND ? OR : AND;
		free_node(nd);
		return n;
	case NOT:
		n = n->nd_right;
		nd->nd_right->nd_right = 0;
		kill_node(nd);
		return n;
	case '=':
		free_node(nd);
		n->nd_symb = NOTEQUAL;
		return n;
	case '<':
		free_node(nd);
		n->nd_symb = GREATEREQUAL;
		return n;
	case '>':
		free_node(nd);
		n->nd_symb = LESSEQUAL;
		return n;
	case LESSEQUAL:
		free_node(nd);
		n->nd_symb = '>';
		return n;
	case GREATEREQUAL:
		free_node(nd);
		n->nd_symb = '<';
		return n;
	case NOTEQUAL:
		free_node(nd);
		n->nd_symb = '=';
		return n;
	}
	return nd;
}

static int
is_assign_operator(oper)
	int	oper;
{
	switch(oper) {
	case BECOMES:
	case PLUSBECOMES:
	case MINBECOMES:
	case TIMESBECOMES:
	case DIVBECOMES:
	case MODBECOMES:
	case B_ORBECOMES:
	case B_XORBECOMES:
	case B_ANDBECOMES:
	case ORBECOMES:
	case ANDBECOMES:
	case LSHBECOMES:
	case RSHBECOMES:
		return 1;
	}
	return 0;
}

p_node
chk_arithop(left, right, oper)
	p_node	left;
	p_node	right;
	int	oper;
{
	/*	Check a (binary) arithmetic operator. Perform it if the
		operands are constants.
	*/
	p_node	nd;

	if (is_assign_operator(oper)) {
		nd = mk_leaf(Stat, oper);
		nd->nd_desig = left;
		nd->nd_expr = right;
	}
	else {
		nd = mk_expr(Oper, oper, left, right);
	}

	if (left->nd_type == error_type || right->nd_type == error_type) {
		nd->nd_type = error_type;
		return nd;
	}

	nd->nd_type = left->nd_type;
	if (! tst_compat(left->nd_type, right->nd_type)) {
		pos_error(&nd->nd_pos,
			  "%s in operands of %s",
			  incompat(left->nd_type, right->nd_type),
			  symbol2str(oper));
	}
	else if (! (allowed_types(oper) & left->nd_type->tp_fund)
		 || (left->nd_type != bool_type
		     && (oper == AND
			 || oper == OR
			 || oper == ANDBECOMES
			 || oper == ORBECOMES))) {
		pos_error(&nd->nd_pos,
			  "illegal operand type(s) in %s",
			  symbol2str(oper));
	}
	else {
		if (nd->nd_class != Stat
		    && left->nd_class == Value && right->nd_class == Value) {
			if (left->nd_type->tp_fund == T_REAL) {
				return fcst_arithop(nd);
			}
			return cst_arithop(nd);
		}
		if ((oper == AND || oper == OR) &&
		    (left->nd_class == Value || right->nd_class == Value)) {
			return cst_boolop(nd);
		}
	}
	return nd;
}

p_node
chk_designator(nd)
	p_node	nd;
{
	/*	Check a designator. There are two versions of this routine:
		one for designators that are in some way assigned to, and
		one for designators in expressions. This one is for
		expressions.
	*/
	t_def	*df;

	if (nd->nd_type == error_type) return nd;
	if (nd->nd_type && (nd->nd_type->tp_fund & T_GENPAR) &&
	    generic_actual_of(nd->nd_type)) {
		nd->nd_type = generic_actual_of(nd->nd_type);
	}
	switch(nd->nd_class) {
	case Select:
	case Arrsel:
	case Value:
	case Ofldsel:
		break;
	case Def:
		df = nd->nd_def;
		switch(df->df_kind) {
		case D_ENUM:
			/*	Replace by its value. */
			nd->nd_flags |= ND_CONST;
			nd->nd_class = Value;
			nd->nd_int = df->enm_val;
			df->df_flags |= D_USED;
			break;
		case D_CONST:
			nd->nd_flags |= ND_CONST;
			if (df->df_flags & D_GENERICPAR) {
				nd->nd_flags |= ND_GENERICPAR;
			}
			if (df->con_const) {
				if (df->con_const->nd_class == Aggr) break;
				nd->nd_class = df->con_const->nd_class;
				nd->nd_symb = df->con_const->nd_symb;
				nd->nd_u.nd_Value = df->con_const->nd_u.nd_Value;
				df->df_flags |= D_USED;
			}
			break;
		case D_OFIELD:
			if (! ProcScope ||
			    ! ProcScope->sc_definedby ||
			    ! (ProcScope->sc_definedby->df_kind
			       & (D_OPERATION|D_OBJECT|D_ERROR)) ||
			       in_dependency_section) {
				pos_error(&nd->nd_pos,
					"\"%s\" may only be used in an operation or object initialization",
					nd->nd_def->df_idf->id_text);
			}
			break;
		case D_FUNCTION:
			if (df->df_type == std_type) {
				pos_error(&nd->nd_pos,
					"call of \"%s\": parentheses missing",
					nd->nd_def->df_idf->id_text);
				nd->nd_type = error_type;
				break;
			}
			if (df->df_scope->sc_definedby &&
			    (df->df_scope->sc_definedby->df_flags & D_DATA)) {
				pos_error(&nd->nd_pos,
					"illegal use: function variable for data module function");
				break;
			}
			if (! df->prc_funcno) {
				df->prc_funcno = ++(CurrDef->mod_funcaddrcnt);
				def_enlist(&CurrDef->mod_funcaddrs, df);
				if (! (df->df_flags & D_DEFINED)) {
				    def_enlist(&ProcScope->sc_definedby->bod_transdep, df);
				}
			}
			break;
		case D_VARIABLE:
			if (is_out_param(df) && in_dependency_section) {
				pos_error(&nd->nd_pos,
					"no access to OUT parameter allowed in dependency section");
			}
			if (df->df_flags & D_SELF) {
				pos_error(&nd->nd_pos,
					"SELF may only be used as an operation LHS");
			}
			break;
		case D_ERROR:
			break;
		case D_OPERATION:
			if (in_doldol) break;
			/* Fall through */
		default:
			pos_error(&nd->nd_pos,
				  "\"%s\" is not a designator",
				  nd->nd_def->df_idf->id_text);
			nd->nd_type = error_type;
			break;
		}
		break;
	default:
		pos_error(&nd->nd_pos, "designator expected");
	}
	return nd;
}

static void
chk_assignment_allowed(nd)
	p_node	nd;
{
	/*	Check that an assignment to designator nd is allowed.
	*/
	t_def	*df;

	switch(nd->nd_class) {
	case Ofldsel:
		if (ProcScope && ProcScope->sc_definedby &&
		    (ProcScope->sc_definedby->df_flags & D_PARALLEL)) {
			pos_error(&nd->nd_pos,
				"assignment to non-owned object field is illegal");
		}
		break;
	case Select:
		df = nd->nd_right->nd_def;
		if (df->df_flags & D_DATA) {
			if (df->df_scope->sc_definedby != CurrDef) {
				pos_error(&nd->nd_pos,
					"assignment to data-module field \"%s\" is illegal",
					df->df_idf->id_text);
			}
		}
		if (df->df_kind == D_UFIELD && (df->df_flags & D_TAG)) {
			pos_error(&nd->nd_pos,
				"assignment to union tag field is illegal");
		}
		/* fall through */
	case Arrsel:
		if (nd->nd_flags & ND_CONST) {
			pos_error(&nd->nd_pos,
				  "assignment to constant is illegal");
		}
		chk_assignment_allowed(nd->nd_left);
		break;
	case Def:
		df = nd->nd_def;
		if (df->df_kind == D_OFIELD) {
		    if (df->df_flags & (D_LOWER_BOUND|D_UPPER_BOUND)) {
			pos_error(&nd->nd_pos,
				  "assignment to object dimension \"%s\" is illegal",
				  df->df_idf->id_text);
		    }
		    if ((CurrDef->df_flags & D_PARTITIONED) &&
			(! ProcScope ||
			 (ProcScope->sc_definedby &&
			  ! (ProcScope->sc_definedby->df_flags & D_PARALLEL)))){
			pos_error(&nd->nd_pos,
				"assignment to partitioned-object field \"%s\"  requires index",
				df->df_idf->id_text);
		    }
		}
		if (df->df_kind == D_VARIABLE &&
		    df->df_flags & D_PART_INDEX) {
			pos_error(&nd->nd_pos,
				  "assignment to partition index \"%s\" is illegal",
				  df->df_idf->id_text);
		}
		if (df->df_kind == D_VARIABLE &&
		    (df->df_flags & D_SELF)) {
			pos_error(&nd->nd_pos,
				  "assignment to SELF is illegal");
		}
		if (df->df_kind & (D_CONST|D_ENUM)) {
			pos_error(&nd->nd_pos,
				  "assignment to %s \"%s\" is illegal",
				  df->df_kind == D_CONST ?
					"constant" :
					"enumeration literal",
				  df->df_idf->id_text);
		}
		if (df->df_flags & D_FORLOOP) {
			pos_error(&nd->nd_pos,
				"assignment to loop variable \"%s\" is illegal",
				df->df_idf->id_text);
		}
		if (df->df_flags & D_DATA) {
			if (df->df_scope->sc_definedby != CurrDef) {
				pos_error(&nd->nd_pos,
					"assignment to data-module field \"%s\" is illegal",
					df->df_idf->id_text);
			}
		}
		break;
	}
}

static void
chk_is_ass_desig(nd)
	p_node	nd;
{
	/*	Check a designator. There are two versions of this routine:
		one for designators that are in some way assigned to, and
		one for designators in expressions. This one is for
		assignments.
	*/

	if (nd->nd_type == error_type) return;
	if (nd->nd_flags & ND_NODESIG) {
		pos_error(&nd->nd_pos, "designator expected");
		return;
	}
	switch(nd->nd_class) {
	case Def:
		nd = chk_designator(nd);
		/* fall through */
	case Ofldsel:
	case Select:
	case Arrsel:
		chk_assignment_allowed(nd);
		break;
	default:
		pos_error(&nd->nd_pos, "designator expected");
	}
}

p_node
chk_aggregate(l, tp)
	p_node	l;
	t_type	*tp;
{
	/*	Check an aggregate: check that the members have the appropriate
		type, and that the number of members is correct.
		Also, if all members are constants, mark the aggregate as a
		constant.
	*/

	p_node	nd = mk_leaf(Aggr, '{');
	int	is_const = ND_CONST;
	int	nel = 0;
	p_node	exp;

	nd->nd_memlist = l;
	mark_defs(nd, D_USED);
	nd->nd_type = tp;
	if (tp == error_type) return nd;
	switch(tp->tp_fund) {
	case T_SET:
	case T_BAG:
		tp = element_type_of(tp);
		node_walklist(nd->nd_memlist, l, exp) {
			nel++;
			is_const &= exp->nd_flags & ND_CONST;
			if (! tst_ass_compat(tp, exp->nd_type)) {
				pos_error(&exp->nd_pos,
					"type incompatibility in %s aggregate",
					tp->tp_fund == T_SET ? "set" : "bag");
			}
		}
		nd->nd_nelements = nel;
		break;

	case T_RECORD: {
		t_def *df = tp->rec_scope->sc_def;

		while (df) {
			if (node_emptylist(l)) {
				pos_error(&nd->nd_pos,
					"record aggregate has too few members");
				break;
			}
			exp = node_getlistel(l);
			l = node_nextlistel(l);
			is_const &= exp->nd_flags & ND_CONST;
			if (! tst_ass_compat(df->df_type, exp->nd_type)) {
				pos_error(&exp->nd_pos,
				  "type incompatibility in record aggregate");
			}
			df = df->df_nextinscope;
		}
		if (! node_emptylist(l)) {
			pos_error(&nd->nd_pos,
				  "record aggregate has too many members");
		}
		nd->nd_flags |= is_const;
		}

		break;

	case T_UNION: {
		t_def *df;

		if (node_emptylist(l)) {
			pos_error(&nd->nd_pos,
				  "union aggregate has too few members");
			break;
		}
		exp = node_getlistel(l);
		if (exp->nd_class != Value) {
			pos_error(&exp->nd_pos,
			  "tag-expression in union aggregate must be constant");
			break;
		}
		df = get_union_variant(tp, exp);
		l = node_nextlistel(l);
		if (node_emptylist(l)) {
			pos_error(&nd->nd_pos,
				  "union aggregate has too few members");
			break;
		}
		exp = node_getlistel(l);
		is_const &= exp->nd_flags & ND_CONST;
		l = node_nextlistel(l);
		if (df
		    && ! tst_ass_compat(df->df_type, exp->nd_type)) {
			pos_error(&exp->nd_pos,
				  "type incompatibility in union aggregate");
		}
		if (! node_emptylist(l)) {
			pos_error(&nd->nd_pos,
				  "union aggregate has too many members");
		}
		nd->nd_flags |= is_const;
		}
		break;
	default:
		pos_error(&nd->nd_pos, "illegal type specifier in aggregate");
		nd->nd_type = error_type;
	}
	return nd;
}

p_node
chk_array_aggregate(nd, tp)
	p_node	nd;
	t_type	*tp;
{
	p_node	l;
	p_node	exp;
	int	nel = 0;
	int	is_const = ND_CONST;
	int	oldnel = -1;
	static int
		level = 1;

	if (level == 1) {
		exp = nd;
		nd = mk_leaf(Aggr, '[');
		if (tp != error_type && tp->tp_fund != T_ARRAY) {
			error("illegal type specifier in array aggregate");
			tp = error_type;
		}
		nd->nd_type = tp;
		nd->nd_memlist = exp;
	}

	if ((tp->tp_flags & T_HASBNDS) &&
	    ! (tp->tp_flags & T_CONSTBNDS)) {
		/* ???
		   The array type specifies bounds, but these cannot be
		   evaluated at compile-time.
		   This situation needs work. We need runtime checks here
		   for the size of the aggregate instead of an error message.
		*/
		pos_error(&nd->nd_pos,
			  "aggregate not allowed for an array-type with fixed, non-constant bounds");
		return nd;
	}
	node_walklist(nd->nd_memlist, l, exp) {
		nel++;
		if (exp->nd_class == Row) {
			if (tp != error_type && level == tp->arr_ndim) {
				pos_error(&exp->nd_pos,
					  "too many dimensions in array aggregate");
				tp = error_type;
			}
			/* Check that each row has the correct number of
			   elements.
			*/
			if (oldnel == -1 && (tp->tp_flags & T_CONSTBNDS)) {
			    oldnel = tp->arr_bounds(level)->nd_right->nd_int -
				tp->arr_bounds(level)->nd_left->nd_int + 1;
			}
			level++;
			(void) chk_array_aggregate(exp, tp);
			level--;
			is_const &= exp->nd_flags & ND_CONST;
			if (oldnel == -1) {
				oldnel = exp->nd_nelements;
			}
			else if (exp->nd_nelements != oldnel) {
				pos_error(&exp->nd_pos,
					  "%s number of array elements in array aggregate",
					  (tp->tp_flags & T_CONSTBNDS) ? "wrong" : "inconsistent");
			}
		}
		else {
			if (tp != error_type && level != tp->arr_ndim) {
				pos_error(&exp->nd_pos,
					"too few dimensions in array aggregate");
				tp = error_type;
			}
			if (tp != error_type &&
			    ! tst_ass_compat(element_type_of(tp), exp->nd_type)) {
				pos_error(&exp->nd_pos,
				    "type incompatibility in array aggregate");
			}
		}
	}
	if (tp != error_type &&
	    tp->arr_index(level-1)->tp_fund == T_ENUM
	    && nel > tp->arr_index(level-1)->enm_ncst) {
		pos_error(&exp->nd_pos,
			  "too many elements for index type");
	}
	nd->nd_nelements = nel;
	nd->nd_flags |= is_const;
	return nd;
}

static void
df_error(mess, df, pos)
	char	*mess;
	t_def	*df;
	t_pos	*pos;
{
	if (df) {
		if (df->df_kind != D_ERROR) {
			pos_error(pos, "\"%s\": %s", df->df_idf->id_text, mess);
		}
		/* else no message */
	}
	else pos_error(pos, mess);
}

static p_node
getarg(argp, type_funds, designator, df)
	p_node	*argp;
	int	type_funds;
	int	designator;
	t_def	*df;
{
	/*	This routine is used to fetch the next parameter from a
		parameter list. The parameter list is indicated by "argp".
		The parameter "type_funds" is a bitset indicating which types
		are allowed at this point, and "designator" is a flag
		indicating that the parameter must indicate a designator.
	*/
	p_node	arglist = *argp;
	p_node	arg;

	if (node_emptylist(arglist)) {
		df_error("too few parameters supplied", df, &dot.tk_pos);
		return 0;
	}

	arg = node_getlistel(arglist);

	if (designator) {
		chk_is_ass_desig(arg);
		if (designator == D_SHAREDPAR) {
			mark_defs(arg, D_USED|D_DEFINED);
		}
		else mark_defs(arg, D_DEFINED);
	}
	else {
		mark_defs(arg, D_USED);
	}
	*argp = node_nextlistel(arglist);
	if (arg->nd_type == error_type) {
	}
	else if (! arg->nd_type
		 || (type_funds && !(type_funds & arg->nd_type->tp_fund))) {
		df_error("unexpected parameter type", df, &arg->nd_pos);
		return 0;
	}
	return arg;
}

static p_node
getname(argp, def_kinds, type_funds, df)
	p_node	*argp;
	int	def_kinds;
	int	type_funds;
	t_def	*df;
{
	/*	This routine is used to fetch the next parameter from an
		parameter list when it must indicate a definition (with
		VAL, MIN or MAX).
		The parameter list is indicated by "argp".
		The parameter "def_kinds" is a bitset indicating which
		kind of definition is expected.
		The parameter "type_funds" is a bitset indicating which types
		are allowed at this point.
	*/
	p_node	arg;

	if (node_emptylist(*argp)) {
		df_error("too few parameters supplied", df, &dot.tk_pos);
		return 0;
	}

	arg = node_getlistel(*argp);
	*argp = node_nextlistel(*argp);
	if (arg->nd_class != Def) {
		df_error("identifier expected", df, &arg->nd_pos);
		return 0;
	}

	if (!(arg->nd_def->df_kind & def_kinds)
	    || (type_funds && !(arg->nd_type->tp_fund & type_funds))) {
		df_error("unexpected parameter type", df, &arg->nd_pos);
		return 0;
	}
	return arg;
}

p_node
chk_funcall(dsg, args)
	p_node	dsg;
	p_node	args;
{
	/*	Check a function call (with a result).
	*/
	p_node	nd;

	nd = chk_call(dsg, args);
	if (! nd->nd_type) {
		pos_error(&nd->nd_pos, "function call expected");
		nd->nd_type = error_type;
	}
	return nd;
}

p_node
chk_proccall(dsg, args)
	p_node	dsg;
	p_node	args;
{
	/*	Check a procedure call (without a result).
	*/
	p_node	nd;

	nd = chk_call(dsg, args);
	if (nd->nd_type && nd->nd_type != error_type) {
		pos_error(&nd->nd_pos, "unexpected function result");
		nd->nd_type = 0;
	}
	return nd;
}

static void
chk_params(params, args, df)
	t_dflst	params;
	p_node	args;
	t_def	*df;
{
	/*	Check the parameters of a function or operation call, or
		a fork.
	*/

	int	cnt;
	t_dflst	par1, par2;
	p_node	arg1p, arg2p;

	arg2p = args;
	par2 = par1 = params;
	for (cnt=1; ! def_emptylist(par1); cnt++, par1=def_nextlistel(par1)) {
		t_def *df1 = def_getlistel(par1);
		int designator = df1->df_flags & (D_OUTPAR|D_SHAREDPAR);
		p_node	arg = getarg(&args, 0, designator, df);
		if (! arg) {
			/* Too few parameters supplied. Error message is
			   produced in getarg().
			*/
			return;
		}
		if (df1->df_flags & D_GATHERED) {
		     chk_par_compat(cnt, !designator, df1->var_gathertp,
				arg->nd_type, df);
		}
		else chk_par_compat(cnt, !designator, df1->df_type,
			       arg->nd_type, df);
		if (designator == D_SHAREDPAR && df->df_kind == D_PROCESS) {
			mark_defs(arg, D_SHAREDOBJ);
		}
	}

	if (! node_emptylist(args)) {
		/* Too many parameters supplied. Check those as if they are
		   IN parameters.
		*/
		df_error("too many parameters supplied",
			df,
			&(node_getlistel(args)->nd_pos));
		while (! node_emptylist(args)) {
			(void) getarg(&args, 0, 0, df);
		}
		return;
	}

	/* For each pair of parameters (P1,P2), check if P1 and P2
	 * are aliases.
	 */
	for (par1 = par2, arg1p = arg2p; ! def_emptylist(par1);
	     par1 = def_nextlistel(par1), arg1p = node_nextlistel(arg1p)) {
		for (par2 = def_nextlistel(par1), arg2p = node_nextlistel(arg1p);
		     ! def_emptylist(par2);
		     par2 = def_nextlistel(par2), arg2p = node_nextlistel(arg2p)) {
			t_def *df1 = def_getlistel(par1);
			t_def *df2 = def_getlistel(par2);
			if (!(is_in_param(df1) && is_in_param(df2))) {
			      (void) chk_aliasing(node_getlistel(arg1p),
					node_getlistel(arg2p), df1, df2, 1);
			}
		}
	}
}

int
chk_aliasing(arg1, arg2, df1, df2, warn)
	p_node	arg1, arg2;
	t_def	*df1, *df2;
	int	warn;
{
	/* See if the two parameters are possible aliases of each other;
	   If they are aliases, generate an error message. If a run-time
	   check is required, give a warning if warn is set.

	   Things are complicated, because parameters (designators)
	   should be compared left-to-right. For example, if R is a record
	   variable and f a fieldname, then "R" and "R.f" are aliases.
	   However, the compiler stores designators in a bottom-up way,
	   so the root nodes of R and R.f are different. We use the auxiliary
	   routines cmp_nodes, cmp_designators, prevnode, and select_base for
	   solving this problem.
	*/

	if (arg1->nd_class != Def
	    && arg1->nd_class != Select
	    && arg1->nd_class != Arrsel) return DIFFERENT;
	if (arg2->nd_class != Def
	    && arg2->nd_class != Select
	    && arg2->nd_class != Arrsel) return DIFFERENT;
	switch (cmp_designators(arg1, arg2)) {
	case DIFFERENT:
		return DIFFERENT;
	case SIMILAR:
		if (is_in_param(df1)) arg1->nd_flags |= ND_ALIAS;
		else if (is_in_param(df2)) arg2->nd_flags |= ND_ALIAS;
		else {
			if (warn) {
				pos_warning(&arg1->nd_pos, "anti-alias check required");
			}
			arg1->nd_flags |= ND_ALIAS;
			arg2->nd_flags |= ND_ALIAS;
		}
		return SIMILAR;
	case SAME:
		if (is_in_param(df1)) arg1->nd_flags |= ND_ALIAS;
		else if (is_in_param(df2)) arg2->nd_flags |= ND_ALIAS;
		else pos_error(&arg1->nd_pos, "illegal alias");
		return SAME;
	}
	/*NOTREACHED*/
	return DIFFERENT;
}

static int
cmp_nodes(arg1, arg2)
	p_node	arg1, arg2;
{
	/*	See the comment in chk_aliasing. This routine compares two
		nodes, assuming that the left-hand sides (if present) have
		already been compared.
	*/
	switch(arg1->nd_class) {
	case Def:
		if (arg2->nd_class != Def) return DIFFERENT;
		if (arg1->nd_def != arg2->nd_def) return DIFFERENT;
		return SAME;

	case Arrsel:
		if (arg2->nd_class != Arrsel) {
			return DIFFERENT;
		}
		return SIMILAR;	/* Needs more work ??? */

	case Select:
		if (arg2->nd_class != Select) {
			return DIFFERENT;
		}
		return cmp_nodes(arg1->nd_right, arg2->nd_right);

	default:
		return DIFFERENT;  /* actual parameter is not a designator */
	}
}

static p_node
prevnode(nodelist, nd)
	p_node	nodelist,
		nd;
{
	/*	Find the node preceeding node "nd" in the given list.
	*/
	p_node	p;

	if (nodelist == nd) return (p_node) 0;
	p = nodelist;
	for (;;) {
		switch(p->nd_class) {
		case Arrsel:
		case Select:
			if (p->nd_left != nd) {
				p = p->nd_left;
				continue;
			}
			break;
		}
		break;
	}
	return p;
}

int
cmp_designators(arg1, arg2)
	p_node	arg1, arg2;
{
	/*	Compare two parameters. Return DIFFERENT if they cannot be
		aliases, SIMILAR if determining that would require a runtime
		test, and SAME if they are aliases.
	*/
	p_node	np1, np2;
	int	result = SAME;

	assert (arg1 != 0 && arg2 != 0);
	if (arg1->nd_class == Tmp) arg1 = arg1->nd_tmpvar->tmp_expr;
	if (arg2->nd_class == Tmp) arg2 = arg2->nd_tmpvar->tmp_expr;
	if (arg1 == 0 || arg2 == 0) return DIFFERENT;
	np1 = select_base(arg1);
	np2 = select_base(arg2);
	while (np1 != 0 && np2 != 0) {
		switch (cmp_nodes(np1, np2)) {
		case DIFFERENT:
			return DIFFERENT;
		case SIMILAR:
			result = SIMILAR;
			break;
		}
		np1 = prevnode(arg1, np1);
		np2 = prevnode(arg2, np2);
	}
	return result;
}

static p_node
chk_call(dsg, args)
	p_node	dsg;
	p_node	args;
{
	/*	Check something that looks like a function call.
		It could also be a call to an Orca built-in, which makes
		this procedure too long.
	*/

	p_node	arg = 0;
	p_node	arglist;
	t_def	*df = 0;
	p_node	nd;

	nd = mk_leaf(Call, '(');
	nd->nd_callee = dsg;
	nd->nd_parlist = args;
	nd->nd_type = error_type;
	mark_defs(dsg, D_USED);

	if (dsg->nd_type != error_type) {
	    if (! dsg->nd_type || dsg->nd_type->tp_fund != T_FUNCTION) {
		pos_error(&dsg->nd_pos, "function expected");
		dsg->nd_type = error_type;
	    }
	    else if (dsg->nd_class == Def
		     && (dsg->nd_def->df_kind & (D_OPERATION|D_PROCESS))) {
		df_error("function expected", dsg->nd_def, &dsg->nd_pos);
		dsg->nd_type = error_type;
	    }
	}

	if (dsg->nd_type == error_type) {
		/* An error occurred. Check parameters anyway, but just as if
		   they are value parameters.
		*/
		while (args) {
			(void) getarg(&args, 0, 0, df);
		}
		return nd;
	}

	if (dsg->nd_type == std_type) {
		/* Orca built-in. */
		assert(dsg->nd_class == Def);
		df = dsg->nd_def;
		arglist = args;
		switch(df->df_stdname) {
		case S_FROM:
			arg = getarg(&arglist, T_SET|T_BAG, D_SHAREDPAR, df);
			if (arg && arg->nd_type != error_type) {
				nd->nd_type = element_type_of(arg->nd_type);
			}
			break;
		case S_SIZE:
			arg = getarg(&arglist, T_SET|T_BAG|T_ARRAY, 0, df);
			nd->nd_type = int_type;
			break;
		case S_LB:
		case S_UB:
			arg = getarg(&arglist, T_ARRAY|T_OBJECT, 0, df);
			if (arg && arg->nd_type->tp_fund == T_OBJECT) {
				/* If object, it must be partitioned. */
				if (! (arg->nd_type->tp_flags & T_PART_OBJ)) {
					df_error("unexpected parameter type",
						 df, &arg->nd_pos);
					arg->nd_type = error_type;
				}
			}
			if (arglist) {
				/* Dimension number. */
				p_node	arg2 =
					getarg(&arglist, T_INTEGER, 0, df);
				p_type	tp = arg->nd_type;

				if (! const_available(arg2)) {
				    df_error("second arg to LB/UB should be constant",
					     df, &arg2->nd_pos);
				    arg2->nd_int = 1;
				}
				else {
				    if (arg2->nd_class == Def) {
					arg2 = arg2->nd_def->con_const;
				    }
				    if (!arg || tp == error_type){
					break;
				    }
				    if (arg2->nd_int > tp->arr_ndim
					|| arg2->nd_int <= 0) {
					df_error("illegal second arg to LB/UB",
						 df, &arg->nd_pos);
					arg2->nd_int = 1;
				    }
				}
				nd->nd_type = tp->arr_index(arg2->nd_int-1);
			}
			else if (arg && arg->nd_type != error_type) {
				nd->nd_type = arg->nd_type->arr_index(0);
			}
			break;
		case S_ADDNODE:
			arg = getarg(&arglist, T_GRAPH, D_SHAREDPAR, df);
			if (arg && arg->nd_type != error_type) {
				nd->nd_type = arg->nd_type->gra_name;
			}
			break;
		case S_DELETENODE: {
			t_type *tp = 0;

			nd->nd_type = 0;
			arg = getarg(&arglist, T_GRAPH, D_SHAREDPAR, df);
			if (arg && arg->nd_type != error_type) {
				tp = arg->nd_type;
			}
			arg = getarg(&arglist, T_NODENAME, 0, df);
			if (arg && tp &&
			    named_type_of(arg->nd_type) != tp) {
				df_error("incompatible second operand type", df,
					 &arg->nd_pos);
			}
			}
			break;
		case S_INSERT:
		case S_DELETE: {
			t_type *tp = 0;

			nd->nd_type = 0;
			arg = getarg(&arglist, 0, 0, df);
			if (arg && arg->nd_type != error_type) {
				tp = arg->nd_type;
			}
			arg = getarg(&arglist, T_SET|T_BAG, D_SHAREDPAR, df);
			if (arg && tp &&
			    ! tst_type_equiv(element_type_of(arg->nd_type),
					     tp)) {
				df_error("incompatible first operand type", df,
					 &arg->nd_pos);
			}
			}
			break;
		case S_WRITELN:
		case S_WRITE:
		case S_READ:
			CurrDef->df_flags |= D_INOUT_NEEDED;
			while (! node_emptylist(arglist)) {
				arg = getarg(&arglist, (T_REAL|T_INTEGER|T_ENUM|T_ARRAY), df->df_stdname == S_READ ? D_OUTPAR : 0, df);
				if (arg
				    && ((arg->nd_type->tp_fund == T_ARRAY
					 && arg->nd_type != string_type)
					|| (arg->nd_type->tp_fund == T_ENUM
					    && arg->nd_type != char_type))) {
					df_error("unexpected parameter type",
						 df,
						 &arg->nd_pos);
				}
			}
			nd->nd_type = 0;
			return nd;
		case S_STRATEGY:
			nd->nd_type = 0;
			arg = getarg(&arglist, T_OBJECT, D_SHAREDPAR, df);
			if (arg
			    && getarg(&arglist, T_INTEGER, 0, df)
			    && getarg(&arglist, T_INTEGER, 0, df)) {
				mark_defs(arg, D_SHAREDOBJ);
			}
			break;
		case S_ABS:
			arg = getarg(&arglist, T_REAL|T_INTEGER, 0, df);
			if (arg) nd->nd_type = arg->nd_type;
			break;
		case S_CAP:
			nd->nd_type = char_type;
			arg = getarg(&arglist, T_ENUM, 0, df);
			if (arg
			    && arg->nd_type != error_type
			    && arg->nd_type != char_type) {
				df_error("unexpected parameter type",
					df,
					&arg->nd_pos);
			}
			break;
		case S_CHR:
			nd->nd_type = char_type;
			arg = getarg(&arglist, T_INTEGER, 0, df);
			break;
		case S_MAX:
		case S_MIN:
			if (!(arg=getname(&arglist, D_TYPE, T_DISCRETE, df))) {
				break;
			}
			nd->nd_type = arg->nd_type;
			break;
		case S_ODD:
			nd->nd_type = bool_type;
			arg = getarg(&arglist, T_INTEGER, 0, df);
			break;
		case S_ORD:
			nd->nd_type = int_type;
			arg = getarg(&arglist, T_DISCRETE, 0, df);
			break;
		case S_VAL:
			if (!(arg=getname(&arglist, D_TYPE, T_DISCRETE, df))) {
				break;
			}
			nd->nd_type = arg->nd_type;
			arg = getarg(&arglist, T_INTEGER, 0, df);
			break;
		case S_FLOAT:
			nd->nd_type = real_type;
			arg = getarg(&arglist, T_INTEGER, 0, df);
			break;
		case S_TRUNC:
			nd->nd_type = int_type;
			arg = getarg(&arglist, T_REAL, 0, df);
			break;
		case S_NCPUS:
		case S_MYCPU:
			nd->nd_type = int_type;
			break;
		case S_ASSERT:
			nd->nd_type = 0;
			arg = getarg(&arglist, 0, 0, df);
			if (arg
			    && arg->nd_type != bool_type
			    && arg->nd_type != error_type) {
				pos_error(&arg->nd_pos,
					  "ASSERT expression must be boolean");
			}
			break;
		default:
			crash("chk_call, standard");
		}
		if (!node_emptylist(arglist)) {
			df_error("too many parameters supplied", df,
				 &(node_getlistel(arglist)->nd_pos));
		}
		if (arg) {
			switch(df->df_stdname) {
			case S_SIZE:
			case S_LB:
			case S_UB:
				if (arg->nd_type->tp_flags & T_CONSTBNDS) {
					return cst_call(nd, df->df_stdname);
				}
				break;
			case S_MIN:
			case S_MAX:
				return cst_call(nd, df->df_stdname);
			}
			if (const_available(arg)) {
			    if (df->df_stdname <= MAXI) {
				return cst_call(nd, df->df_stdname);
			    }
			    if (df->df_stdname <= MAXIF) {
				if (arg->nd_type->tp_fund != T_REAL) {
					return
					  cst_call(nd, df->df_stdname);
				}
			    }
			    if (df->df_stdname <= MAXF) {
				return fcst_call(nd, df->df_stdname);
			    }
			}
		}
		return nd;
	}
	if (dsg->nd_class == Def) df = dsg->nd_def; else df = 0;
	if (! df || (df->df_flags & D_GENERICPAR) || df->df_kind == D_VARIABLE) {
		if (ProcScope->sc_definedby) {
			ProcScope->sc_definedby->df_flags |= D_CALLS_OP;
		}
	}
	nd->nd_type = result_type_of(dsg->nd_type);
	if (nd->nd_type && (nd->nd_type->tp_fund & T_GENPAR) &&
	    generic_actual_of(nd->nd_type)) {
		nd->nd_type = generic_actual_of(nd->nd_type);
	}
	chk_params(param_list_of(dsg->nd_type), args, df);
	return nd;
}

static p_node
chk_operation(dsg, id, args)
	p_node	dsg;
	t_idf	*id;
	p_node	args;
{
	/*	Check an operation call (mostly checking of parameters).
	*/

	t_type	*tp = error_type;
	p_node	nd = mk_leaf(Call, '$');
	t_def	*df = 0;

	nd->nd_obj = dsg;
	nd->nd_type = error_type;
	nd->nd_parlist = args;

	if (dsg->nd_class == Def &&
	    dsg->nd_def->df_kind == D_VARIABLE &&
	    (dsg->nd_def->df_flags & D_SELF)) {
		/* This is OK as long as it is within an operation. */
		if (! (ProcScope->sc_definedby->df_kind
		       & (D_OPERATION|D_OBJECT|D_ERROR))) {
			pos_error(&dsg->nd_pos,
				"SELF may only be used in an operation");
		}
	}
	else dsg = chk_designator(dsg);
	nd->nd_type = error_type;
	if (dsg->nd_type != error_type) {
	    if (! dsg->nd_type || dsg->nd_type->tp_fund != T_OBJECT) {
		pos_error(&dsg->nd_pos, "object expected");
	    }
	    else {
		df = lookup(id, record_type_of(dsg->nd_type)->rec_scope, 0);
		if (! df || df->df_kind != D_OPERATION) {
		    pos_error(&nd->nd_pos,
			      "\"%s\": no operation",
			      id->id_text);
		    df = 0;
		}
	    }
	}

	if (df) {
	    /* The object type could be imported but never explicitly mentioned.
	       In this case, the D_USED flag would not be set. Therefore, we
	       look it up here.
	    */
	    (void) lookfor(df->df_scope->sc_definedby->df_idf, CurrentScope, 0);
	    if (df->df_flags & D_HASWRITES) {
		/*	In general, we don't have this information available
			yet, so we do it again in simplify.c.
		*/
		mark_defs(dsg, D_USED|D_DEFINED);
	    }
	    else	mark_defs(dsg, D_USED);
	    tp = df->df_type;
	    df->df_flags |= D_USED;
	    nd->nd_callee = mk_leaf(Def, IDENT);
	    nd->nd_callee->nd_def = df;
	    nd->nd_callee->nd_pos = nd->nd_pos;
	    nd->nd_callee->nd_type = df->df_type;
	}

	if (tp == error_type) {
		/* An error occurred. Check parameters anyway, but just as if
		   they are value parameters.
		*/
		while (! node_emptylist(args)) {
			(void) getarg(&args, 0, 0, df);
		}
		return nd;
	}

	assert(tp->tp_fund == T_FUNCTION);
	nd->nd_type = result_type_of(tp);
	if (nd->nd_type && (nd->nd_type->tp_fund & T_GENPAR) &&
	    generic_actual_of(nd->nd_type)) {
		nd->nd_type = generic_actual_of(nd->nd_type);
	}
	chk_params(param_list_of(tp), args, df);
	return nd;
}

p_node
chk_funopcall(dsg, id, parlist)
	p_node	dsg;
	t_idf	*id;
	p_node	parlist;
{
	p_node	nd;

	nd = chk_operation(dsg, id, parlist);
	if (! nd->nd_type) {
		pos_error(&nd->nd_pos, "value-returning operation expected");
		nd->nd_type = error_type;
	}
	return nd;
}

p_node
chk_procopcall(dsg, id, parlist)
	p_node	dsg;
	t_idf	*id;
	p_node	parlist;
{
	p_node	nd;

	nd = chk_operation(dsg, id, parlist);
	if (nd->nd_type && nd->nd_type != error_type) {
		pos_error(&nd->nd_pos, "unexpected operation result");
		nd->nd_type = 0;
	}
	return nd;
}

p_node
chk_assign(nd, rhs, oper)
	p_node	nd;
	p_node	rhs;
	int	oper;
{
	/*	Check an assignment statement. The lhs (designator) must be
		"assignable". For an ordinary assignment, the type of the rhs
		must be assigmnent compatible with that of the lhs. For an
		assignment operator, chk_arithop does the work.
	*/
	p_node	nd1;

	chk_is_ass_desig(nd);
	mark_defs(nd, D_DEFINED);
	if (oper == BECOMES) {
		if (! tst_ass_compat(nd->nd_type,
				     rhs->nd_type)) {
			pos_error(&nd->nd_pos,
				  "type incompatibility in assignment");
		}
		mark_defs(rhs, D_USED);
		nd1 = mk_leaf(Stat, oper);
		nd1->nd_desig = nd;
		nd1->nd_expr = rhs;
		nd1->nd_type = 0;
		return nd1;
	}
	mark_defs(rhs, D_USED);
	nd1 = chk_arithop(nd, rhs, oper);
	nd1->nd_type = 0;
	return nd1;
}

p_node
chk_fork(dsg, arg, on_expr)
	p_node	dsg;
	p_node	arg;
	p_node	on_expr;
{
	/*	Check a FORK statement. The designator must indicate a process,
		parameters must be correct, and if there is an ON expression,
		it must be an integer.
	*/

	p_node	nd;
	t_def	*df = 0;

	nd = mk_leaf(Call, FORK);
	nd->nd_target = on_expr;
	nd->nd_callee = dsg;
	nd->nd_parlist = arg;
	if (! (ProcScope->sc_definedby->df_kind & (D_PROCESS|D_ERROR))) {
		error("FORK statement only allowed within a process");
	}
	ProcScope->sc_definedby->df_flags |= D_HASFORKS;
	mark_defs(dsg, D_USED);
	if (dsg->nd_class != Def ||
	    !(dsg->nd_def->df_kind & (D_PROCESS|D_ERROR))) {
		pos_error(&dsg->nd_pos, "process expected");
		dsg->nd_type = error_type;
	}
	else df = dsg->nd_def;
	if (dsg->nd_type == error_type) {
		/* An error occurred. Check parameters anyway, but just as if
		   they are value parameters.
		*/
		while (!node_emptylist(arg)) {
			(void) getarg(&arg, 0, 0, df);
		}
		return nd;
	}
	chk_params(param_list_of(dsg->nd_type), arg, df);
	if (on_expr) {
		mark_defs(on_expr, D_USED);
		if (! tst_ass_compat(int_type, on_expr->nd_type)) {
			pos_error(&on_expr->nd_pos,
			  "integer expression expected in ON-clause of FORK");
		}
	}
	return nd;
}

p_node
chk_bat(nd)
	p_node	nd;
{
	/* Check that the expression in "nd" may be used as a bounds-
	   or tag-expression. This means that it must either be a
	   constant expression or only use shared or input parameters.
	*/

	if (nd->nd_type == error_type) return nd;

	switch(nd->nd_class) {
	case Value:
		break;
	case Arrsel:
		nd->nd_expr = chk_bat(nd->nd_expr);
		break;
	case Oper:
		nd->nd_left = chk_bat(nd->nd_left);
		/* fall through */
	case Uoper:
		nd->nd_right = chk_bat(nd->nd_right);
		break;
	case Call:
		if (nd->nd_callee->nd_type == std_type
		    && (nd->nd_callee->nd_def->df_stdname <= MAXF
			|| nd->nd_callee->nd_def->df_stdname == S_NCPUS
			|| nd->nd_callee->nd_def->df_stdname == S_MYCPU)) {
			p_node	lst;
			p_node	n;

			node_walklist(nd->nd_parlist, lst, n) {
#ifndef NDEBUG
				p_node	s = chk_bat(n);
				assert(s == n);
#else
				(void) chk_bat(n);
#endif
			}
			break;
		}
		/* Fall through */
	case Aggr:
	case Row:
		pos_error(&nd->nd_pos, "illegal bounds and/or tag expression");
		break;
	case Def:
		if (! (nd->nd_def->df_flags & (D_INPAR|D_SHAREDPAR|D_DATA))) {
			pos_error(&nd->nd_pos, "\"%s\": bounds and/or tag expression may only use IN or SHARED parameters or constants", nd->nd_def->df_idf->id_text);
		}
		break;
	case Select:
		nd->nd_left = chk_bat(nd->nd_left);
		break;
	default:
		crash("chk_bat");
	}
	return nd;
}

p_node
chk_upto(left, right, tp)
	p_node	left;
	p_node	right;
	t_type	*tp;
{
	/*	Check the left and right side of an UPTO expression,
		in a specification of bounds in an array or partitioned object
		declaration, or in a FOR loop.
		The supposed type of the bounds is in tp.
		Return an UPTO link to both expressions.
	*/
	p_node	nd = mk_expr(Link, UPTO, left, right);

	mark_defs(nd, D_USED);

	/* Check type equivalence between expressions, and between either
	   one and the supposed type.
	*/
	if (! (tp->tp_fund & T_DISCRETE)) {
		pos_error(&left->nd_pos, "type in range must be discrete");
		tp = error_type;
	}
	chk_type_equiv(left->nd_type, right, "range");
	chk_type_equiv(tp, left, "range and range type");

	nd->nd_type = left->nd_type;
	nd->nd_pos = right->nd_pos;

	/* Check if the range is constant. */
	if (left->nd_class == Value && right->nd_class == Value) {
		nd->nd_flags |= ND_CONST;
		if (left->nd_int > right->nd_int) {
			pos_warning(&right->nd_pos, "empty range");
		}
		else {
			nd->nd_count = right->nd_int - left->nd_int + 1;
			nd->nd_flags |= ND_FORDONE;
		}
	}
	return nd;
}

p_node
chk_case(explist, statlist)
	p_node	explist;
	p_node	statlist;
{
	p_node	nd = mk_leaf(Stat, ARROW);

	nd->nd_list1 = explist;
	nd->nd_list2 = statlist;
	return nd;
}

p_node
chk_doldol(dsg, id, args)
	p_node	dsg;
	p_idf	id;
	p_node	args;
{
	p_node	nd;
	p_node	n;
	p_node	l;
	int	ndims;
	char	*txt;
	int	i;

	dsg = chk_designator(dsg);
	if (dsg->nd_type != error_type) {
		if (! dsg->nd_type || dsg->nd_type->tp_fund != T_OBJECT ||
		    !(dsg->nd_type->tp_def->df_flags & D_PARTITIONED)) {
			pos_error(&dsg->nd_pos, "partitioned object expected");
			dsg->nd_type = error_type;
		}
	}

	nd = mk_leaf(Call, DOLDOL);
	nd->nd_obj = dsg;
	nd->nd_callee = mk_leaf(Name, IDENT);
	nd->nd_callee->nd_idf = id;
	nd->nd_parlist = args;

	if (dsg->nd_type == error_type) {
		nd->nd_type = error_type;
		return nd;
	}

	ndims = dsg->nd_type->arr_ndim;
	txt = id->id_text;

	if (! strcmp(txt, "partition")) {
		/* One parameter for each partitioned object dimension. */
		chk_indices(args, dsg->nd_type, "parameter", "parameters");
		return nd;
	}
	if (! strcmp(txt, "distribute")) {
		/* One parameter: and array[integer] of integers. */
		n = node_getlistel(args);
		if (! n) {
			pos_error(&nd->nd_pos, "too few parameters");
			return nd;
		}
		if (node_nextlistel(args)) {
			pos_error(&nd->nd_pos, "too many parameters");
			return nd;
		}
		mark_defs(n, D_USED);
		if (n->nd_type->tp_fund != T_ARRAY ||
		    n->nd_type->arr_ndim != 1 ||
		    n->nd_type->arr_index(0) != int_type ||
		    element_type_of(n->nd_type) != int_type) {
			pos_error(&nd->nd_pos, "type incompatibility in parameter #1");
		}
		return nd;
	}
	if (! strcmp(txt, "distribute_on_n") ||
	    ! strcmp(txt, "distribute_on_list")) {
		i = 0;
		l = args;

		if (! strcmp(txt, "distribute_on_list")) {
			n = l;
			l = node_nextlistel(l);
			i++;

			if (! n) {
				pos_error(&n->nd_pos, "too few parameters");
			}
			else if (n->nd_type->tp_fund != T_ARRAY ||
			    n->nd_type->arr_ndim != 1 ||
			    n->nd_type->arr_index(0) != int_type ||
			    element_type_of(n->nd_type) != int_type) {
				pos_error(&nd->nd_pos, "distribute_on_list: type incompatibility");
			}
			mark_defs(n, D_USED);
		}
		/* Two integer parameters for each dimension. */
		node_walklist(l, l, n) {
			i++;
			if (! tst_compat(n->nd_type, int_type)) {
				pos_error(&n->nd_pos, "type incompatibility in parameter #%d", i);
			}
			mark_defs(n, D_USED);
		}
		if (! strcmp(txt, "distribute_on_list")) {
			i--;
		}
		if (i > 2*ndims) {
			pos_error(&nd->nd_pos, "too many parameters");
		}
		else if (i < 2*ndims) {
			pos_error(&nd->nd_pos, "too few parameters");
		}
		return nd;
	}
	if (! strcmp(txt, "clear_dependencies") ||
	    ! strcmp(txt, "set_dependencies") ||
	    ! strcmp(txt, "add_dependency") ||
	    ! strcmp(txt, "remove_dependency")) {
		n = node_getlistel(nd->nd_parlist);
		l = node_nextlistel(nd->nd_parlist);

		if (! n) {
			pos_error(&nd->nd_pos, "too few parameters");
			return nd;
		}
		mark_defs(n, D_USED);

		if (n->nd_class != Def) {
			pos_error(&n->nd_pos,
				  "type incompatibility in parameter #1");
		}

		if (n->nd_def->df_kind == D_ERROR) return nd;

		if (n->nd_def->df_kind != D_OPERATION ||
		    ! (n->nd_def->df_flags & D_PARALLEL) ||
		    record_type_of(nd->nd_obj->nd_type)->rec_scope != n->nd_def->df_scope) {
			pos_error(&n->nd_pos,
				  "type incompatibility in parameter #1");
			return nd;
		}

		if (! strcmp(txt, "add_dependency") ||
		    ! strcmp(txt, "remove_dependency")) {
		    t_type	*tp = nd->nd_obj->nd_type;
		    int	j;

		    i = 0;
		    j = -1;
		    node_walklist(l, l, n) {
			mark_defs(n, D_USED);
			j++;
			if ( j == ndims) { j = 0; }
			if (i >= 2*ndims) {
			    pos_error(&n->nd_pos, "too many parameters");
			    break;
			}
			i++;
			/* Parameter must be assignment compatible */
			if (! tst_ass_compat(tp->arr_index(j), n->nd_type)) {
			    pos_error(&n->nd_pos, "type incompatibility in parameter #%d", i+1);
			}
		    }
		    if (i < ndims) {
			pos_error(&nd->nd_pos, "too few parameters");
		    }
		}
		else if (l) {
		    pos_error(&nd->nd_pos, "too many parameters");
		}
		return nd;
	}
	return nd;
}

p_node
chk_return(expr)
	p_node	expr;
{
	/*	Perform a number of checks on a RETURN:
		- check the presence/absence of an expression;
		- check the result type for compatibility (which is made
		  a bit more complex because of GATHER;
	*/
	t_type	*result_tp;
	t_def	*df = ProcScope->sc_definedby;
	p_node	nd = mk_leaf(Stat, RETURN);
	struct loop_list
		*loop;

	/* Mark all enclosing loops to have a RETURN in the body. */
	for (loop = looplist; loop; loop = loop->enclosing) {
		loop->loop->nd_flags |= ND_EXIT_OR_RET;
	}

	if (in_dependency_section) {
		error("RETURN not allowed in dependency section");
	}

	if (expr == 0) {
		/* No return expression. */
		if ((df->df_kind & (D_OPERATION|D_FUNCTION)) &&
		    result_type_of(df->df_type)) {
			error("\"%s\" must return a value", df->df_idf->id_text);
		}
		return nd;
	}

	nd->nd_expr = expr;
	mark_defs(expr, D_USED);

	if (df->df_kind & (D_OBJECT|D_MODULE)) {
		error("%s initialization code cannot return a value",
		      df->df_kind == D_OBJECT ? "object" : "data module");
		return nd;
	}

	result_tp = result_type_of(df->df_type);

	if (! result_tp) {
		pos_error(&expr->nd_pos, "\"%s\" has no result value", df->df_idf->id_text);
	}
	else if (result_tp != error_type) {
		if (df->df_kind == D_OPERATION &&
		    (df->df_flags & D_PARALLEL) &&
		    ! df->opr_reducef) {
			assert(result_tp->tp_fund == T_ARRAY);
			if (! tst_ass_compat(element_type_of(result_tp),
					     expr->nd_type)) {
				pos_error(&expr->nd_pos,
					"type incompatibility in RETURN");
			}
		}
		else if (!tst_ass_compat(result_tp, expr->nd_type)) {
			pos_error(&expr->nd_pos,
				"type incompatibility in RETURN");
		}
		df->df_flags |= D_RETURNEXPR;
	}
	return nd;
}

void
start_body(df)
    t_def   *df;
{
    ProcScope = CurrentScope;
    node_initlist(&df->bod_statlist1);
}

void
add_guard(df, nd)
    t_def   *df;
    t_node  *nd;
{
    node_enlist(&df->bod_statlist1, nd);
    df->df_flags |= D_BLOCKING|D_BLOCKINGDONE|D_GUARDS;
}

void
end_body(df, nd)
    t_def   *df;
    t_node  *nd;
{
    switch(df->df_kind) {
    case D_MODULE:
	assert(df->df_flags & D_DATA);
	df->bod_statlist1 = nd;
	DO_DEBUG(options['C'],
		 (printf("data module body:\n"), dump_nodelist(nd, 0)));
	break;
    case D_OBJECT:
	df->bod_statlist1 = nd;
	DO_DEBUG(options['C'],
		 (printf("object body:\n"), dump_nodelist(nd, 0)));
	break;
    case D_OPERATION:
	if (! (df->df_flags & D_GUARDS)) {
	    df->bod_statlist1 = nd;
	}
	else {
	    nd = df->bod_statlist1;
	    if (CurrDef->df_flags & D_PARTITIONED) {
		error("no guards allowed in operations on partitioned objects");
	    }
	}
	DO_DEBUG(options['C'],
		 (printf("operation body:\n"), dump_nodelist(nd, 0)));
	break;
    case D_FUNCTION:
	df->bod_statlist1 = nd;
	DO_DEBUG(options['C'],
		 (printf("function body:\n"), dump_nodelist(nd, 0)));
	break;
    case D_PROCESS:
	df->bod_statlist1 = nd;
	DO_DEBUG(options['C'],
		 (printf("process body:\n"), dump_nodelist(nd, 0)));
	break;
    }
}

void
chk_shape(df, a, ndim, impl)
	t_def	*df;
	t_ardim	*a;
	int	ndim;
	int	impl;
{
	int	i;

	if (! impl) {
		df->df_type->arr_ndim = ndim;
		if (ndim > 0) {
			df->df_flags |= D_PARTITIONED;
			df->df_type->tp_flags |= T_PART_OBJ;
			df->df_type->arr_ind = a;
		}
		return;
	}
	if (ndim > 0) {
	    if (! (df->df_flags * D_PARTITIONED)) {
		error("object specification specifies a non-partitioned object");
	    }
	    else if (ndim != df->df_type->arr_ndim) {
		error("number of dimensions not consistent with specification");
	    }
	}
	else if (df->df_flags & D_PARTITIONED) {
		error("object specification specifies a partitioned object");
	}
	for (i = 0; i < ndim && i < df->df_type->arr_ndim; i++) {
		if (a[i].ar_index != df->df_type->arr_index(i)) {
			error ("index type of dimension %d not consistent with specification", i+1);
		}
	}
	for (i = 0; i < ndim; i++) {
		p_node	nd = a[i].ar_bounds;
		t_def	*b;
		if (nd) {
			assert(nd->nd_class == Link);
			assert(nd->nd_left->nd_class == Name);
			assert(nd->nd_right->nd_class == Name);

			b = define(nd->nd_left->nd_idf, CurrentScope, D_OFIELD);
			b->df_flags |= D_LOWER_BOUND|D_DEFINED;
			b->fld_dimno = i;
			b->df_type = a[i].ar_index;

			b = define(nd->nd_right->nd_idf, CurrentScope, D_OFIELD);
			b->df_flags |= D_UPPER_BOUND|D_DEFINED;
			b->fld_dimno = i;
			b->df_type = a[i].ar_index;
		}
	}
	free(df->df_type->arr_ind);
	df->df_type->arr_ndim = ndim;
	df->df_type->arr_ind = a;
}
