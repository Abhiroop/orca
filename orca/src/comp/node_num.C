/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* N O D E   N U M B E R I N G */

/* $Id: node_num.C,v 1.12 1997/05/15 12:02:33 ceriel Exp $ */

#include	"debug.h"
#include	"ansi.h"

#include	<alloc.h>
#include	<assert.h>

#include	"node_num.h"
#include	"LLlex.h"
#include	"visit.h"
#include	"temps.h"
#include	"flexarr.h"

_PROTOTYPE(static void num_stat, (p_node));
_PROTOTYPE(static int num_exp, (p_node));
_PROTOTYPE(static int clear_num, (p_node));
_PROTOTYPE(static int clear_expnum, (p_node));
_PROTOTYPE(static int hash_exp, (p_node));
_PROTOTYPE(static int compare_node, (p_node, p_node));
_PROTOTYPE(static void add, (p_node));

/* Statement numbering: basically, just give each statement a different number.
   However, flow control statements may need extra numbers for intermediate
   positions.
   We number the statements basically in depth-first order. This should help
   for data flow analysis, which can then just use the natural ordering
   0 1 2 3 ...
*/

static int	stat_cnt;
static p_flex	list;

static void add(lnd)
	p_node	lnd;
{
	p_node	*f = flex_next(list);
	*f = new_node();
	**f = *lnd;
}

static int
clear_num(nd)
	p_node	nd;
{
	nd->nd_nodenum = 0;
	return 0;
}

static int
clear_expnum(nd)
	p_node	nd;
{
	if (nd->nd_class != Call || nd->nd_type != 0) {
		/* Calls with nd->nd_type == 0 are not expressions. */
		nd->nd_nodenum = 0;
	}
	return 0;
}

static void
num_stat(nd)
	p_node	nd;
{
	p_node	l;
	p_node	n;

	if (nd->nd_symb != ARROW) {
		/* For ARROW, no place holders are needed. */
		nd->nd_nodenum = ++stat_cnt;
		add(nd);
	}
	if (nd->nd_class == Stat) nd->nd_o_info = new_nd_optim();

	/* For complex statements, descend further, but controlled:
	   we only want to number statements.
	*/
	switch(nd->nd_symb) {
	case FOR:
		/* Allocate two extra place holders: one for the
		   position right after the initialization, and one
		   for the position where it is determined that the
		   loop is executed at least once.
		*/
		nd->nd_o_info->o_ndnum[0] = ++stat_cnt;
		add(nd);
		nd->nd_o_info->o_ndnum[1] = ++stat_cnt;
		add(nd);
		break;
	case COND_EXIT:
	case IF:
	case CASE:
	case GUARD:
		/* Allocate an extra place holder for these. See also comments
		   in bld_graph.c.
		*/
		nd->nd_o_info->o_ndnum[0] = ++stat_cnt;
		add(nd);
		break;
	case RETURN:
		/* Extra place holder after return expression. */
		if (nd->nd_expr) {
			nd->nd_o_info->o_ndnum[0] = ++stat_cnt;
			add(nd);
		}
		break;
	}

	if (nd->nd_class == Call) return;

	if (nd->nd_symb != ARROW) {
		node_walklist(nd->nd_list1, l, n) {
			num_stat(n);
		}
	}
	node_walklist(nd->nd_list2, l, n) {
		num_stat(n);
	}
}

int
number_statements(stlist, statentries)
	p_node	stlist;
	p_node	**statentries;
{
	p_node	l;
	p_node	nd;
	p_node	*f;

	list = flex_init(sizeof(p_node), 10);
	f = flex_next(list);
	*f = 0;
	stat_cnt = 0;
	visit_ndlist(stlist, Stat|Call, clear_num, 0);
	node_walklist(stlist, l, nd) {
		num_stat(nd);
	}
	f = flex_next(list);
	*f = 0;
	*statentries = flex_finish(list, (unsigned int *) 0);
	return stat_cnt;
}

/* Expression numbering is more complicated: we want identical expressions
   without side effects to have the same number.
*/

static int exp_cnt;

/* For now, we number expression nodes with nd_class Uoper, Oper, Select,
   Arrsel, Ofldsel, or Check.
*/

static int
compare_node(n1, n2)
	p_node	n1, n2;
{
	if (! n1 && ! n2) return 1;
	if (! n1 || ! n2) return 0;
	if (n1->nd_class != n2->nd_class) return 0;

	if (n1->nd_nodenum && n2->nd_nodenum) {
		return n1->nd_nodenum == n2->nd_nodenum;
	}
	if (n1->nd_class == Tmp) n1 = n1->nd_tmpvar->tmp_expr;
	if (n2->nd_class == Tmp) n2 = n2->nd_tmpvar->tmp_expr;
	switch(n1->nd_class) {
	case Def:
		return n1->nd_def == n2->nd_def;
	case Oper:
		/* The Oper case could be extended for commutative operators.
		   ???
		*/
		if (n1->nd_symb == n2->nd_symb) {
			return	compare_node(n1->nd_left, n2->nd_left) &&
				compare_node(n1->nd_right, n2->nd_right);
		}
		return 0;
	case Arrsel:
	case Select:
	case Ofldsel:
		return n1->nd_symb == n2->nd_symb &&
			compare_node(n1->nd_left, n2->nd_left) &&
			compare_node(n1->nd_right, n2->nd_right);
	case Uoper:
		if (n1->nd_symb == ARR_SIZE && n1->nd_dimno != n2->nd_dimno) {
			return 0;
		}
		return n1->nd_symb == n2->nd_symb &&
			compare_node(n1->nd_right, n2->nd_right);
	case Value:
		if (n1->nd_type == n2->nd_type &&
		    n1->nd_type->tp_fund & (T_INTEGER|T_ENUM)) {
			return n1->nd_int == n2->nd_int;
		}
		break;
	case Check:
		return (n1->nd_symb == n2->nd_symb &&
			(n1->nd_symb != A_CHECK || (n1->nd_dimno==n2->nd_dimno)) &&
			compare_node(n1->nd_left, n2->nd_left) &&
			compare_node(n1->nd_right, n2->nd_right));
	case Call:
		if (n1->nd_symb == n2->nd_symb &&
		    compare_node(n1->nd_callee, n2->nd_callee) &&
		    compare_node(n1->nd_obj, n2->nd_obj)) {
			p_node	l1, l2;

			l2 = n2->nd_parlist;
			node_walklist(n1->nd_parlist, l1, n1) {
				n2 = node_getlistel(l2);
				if (! compare_node(n1, n2)) {
					return 0;
				}
				l2 = node_nextlistel(l2);
			}
			return 1;
		}
		break;
	}
	return 0;
}

#define N_BUCKETS	32
#define MASK_BUCKET	(N_BUCKETS-1)

typedef struct exp_list {
	struct exp_list	*e_next;
	p_node		e_exp;
} exp_list;

/* STATICALLOCDEF "exp_list" 10 */

static exp_list *exp_buckets[N_BUCKETS];

static int
hash_exp(nd)
	p_node	nd;
{
	int	val = 0;

	switch(nd->nd_class) {
	case Check:
	case Oper:
	case Arrsel:
	case Ofldsel:
	case Select:
		val = nd->nd_symb
			+ (nd->nd_left ? (nd->nd_left->nd_nodenum << 3) : 0)
			+ (nd->nd_right->nd_nodenum << 5);
		break;
	case Uoper:
		val = nd->nd_symb + (nd->nd_right ? (nd->nd_right->nd_nodenum << 4) : 0);
		break;
	case Tmp:
		if (nd->nd_flags & ND_LHS) break;
		if (nd->nd_tmpvar->tmp_expr) {
			return hash_exp(nd->nd_tmpvar->tmp_expr);
		}
		break;
	case Call:
		if (! nd->nd_callee) val = nd->nd_symb;
		else if (nd->nd_callee->nd_type == std_type) {
			val = nd->nd_callee->nd_def->df_stdname << 2;
		}
		else if (nd->nd_callee->nd_class == Def) {
			val = nd->nd_callee->nd_def->df_position.pos_lineno<<2;
		}
		else	val = nd->nd_symb;
	}
	return (val >> 2) & MASK_BUCKET;
}

static int
num_exp(nd)
	p_node	nd;
{
	int	hashval = hash_exp(nd);
	exp_list
		*ep = exp_buckets[hashval];

	if (nd->nd_nodenum) return 0;

	if (nd->nd_class == Call && ! nd->nd_type) return 0;

	if (nd->nd_class == Tmp) {
		if (nd->nd_tmpvar->tmp_expr) {
			visit_node(nd->nd_tmpvar->tmp_expr,
				   Oper|Arrsel|Ofldsel|Select|Uoper|Check|Tmp|Call,
				   num_exp,
				   0);
			nd->nd_nodenum = nd->nd_tmpvar->tmp_expr->nd_nodenum;
		}
		return 0;
	}

	while (ep && ! compare_node(ep->e_exp, nd)) {
		ep = ep->e_next;
	}
	if (ep) {
		nd->nd_nodenum = ep->e_exp->nd_nodenum;
		return 0;
	}
	nd->nd_nodenum = ++exp_cnt;
	add(nd);
	ep = new_exp_list();
	ep->e_next = exp_buckets[hashval];
	ep->e_exp = nd;
	exp_buckets[hashval] = ep;
	return 0;
}

int
number_expressions(elist, exprs)
	p_node	elist;
	p_node	**exprs;
{
	int	i;
	p_node	*f;

	list = flex_init(sizeof(p_node), 10);
	f = flex_next(list);
	*f = 0;
	exp_cnt = 0;
	for (i = 0; i < N_BUCKETS; i++) {
		exp_list *ep = exp_buckets[i];
		while (ep) {
			exp_list *ep1 = ep->e_next;
			free_exp_list(ep);
			ep = ep1;
		}
		exp_buckets[i] = 0;
	}
	visit_ndlist(elist,
		     Oper|Arrsel|Ofldsel|Select|Uoper|Check|Tmp|Call,
		     clear_expnum,
		     1);
	visit_ndlist(elist,
		     Oper|Arrsel|Ofldsel|Select|Uoper|Check|Tmp|Call,
		     num_exp,
		     1);

	f = flex_next(list);
	*f = 0;
	*exprs = flex_finish(list, (unsigned int *) 0);
	return exp_cnt;
}
