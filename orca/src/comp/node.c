/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* N O D E   O F   A N	 A B S T R A C T   P A R S E T R E E */

/* $Id: node.c,v 1.21 1997/05/15 12:02:31 ceriel Exp $ */

#include	"debug.h"
#include	"ansi.h"

#include	<alloc.h>
#include	<stdio.h>
#include	<assert.h>

#include	"LLlex.h"
#include	"node.h"
#include	"temps.h"
#include	"type.h"

_PROTOTYPE(static void fill_dot, (p_node));

p_node
mk_expr(class, symb, left, right)
	p_node	left,
		right;
	int	class,
		symb;
{
	/*	Create a node and initialize it with the given parameters
	*/
	p_node	nd = new_node();

	nd->nd_left = left;
	nd->nd_right = right;
	nd->nd_symb = symb;
	nd->nd_pos = dot.tk_pos;
	nd->nd_class = class;
	return nd;
}

void
add_row(memlist, list)
	p_node	*memlist;
	p_node	list;
{
	p_node	nd = mk_leaf(Row, '[');

	nd->nd_memlist = list;
	node_enlist(memlist, nd);
}

static void
fill_dot(nd)
	p_node	nd;
{
	nd->nd_symb = DOT;
	switch(DOT) {
	case STRING:
		nd->nd_str = dot.tk_string;
		nd->nd_type = string_type;
		break;
	case IDENT:
		nd->nd_idf = dot.tk_idf;
		break;
	case INTEGER:
		nd->nd_int = dot.tk_int;
		nd->nd_type = univ_int_type;
		break;
	case CHARACTER:
		nd->nd_int = dot.tk_int;
		nd->nd_type = char_type;
		break;
	case REAL:
		nd->nd_real = dot.tk_real;
		nd->nd_type = univ_real_type;
		break;
	case FROM:
		assert(nd->nd_class == Def);
		nd->nd_def = dot.tk_idf->id_def;
		nd->nd_type = nd->nd_def->df_type;
		break;
	}
}

p_node
mk_leaf(class, symb)
	int	class,
		symb;
{
	p_node	nd = new_node();

	nd->nd_symb = symb;
	nd->nd_pos = dot.tk_pos;
	nd->nd_class = class;
	if (class == Value) nd->nd_flags = ND_CONST;
	return nd;
}

p_node
dot2leaf(class)
	int	class;
{
	p_node	nd = new_node();

	nd->nd_pos = dot.tk_pos;
	nd->nd_class = class;
	fill_dot(nd);
	if (class == Value) nd->nd_flags = ND_CONST;
	return nd;
}

void
kill_nodelist(l)
	p_node	l;
{
	p_node	l1;
	p_node	nd;

	node_walklist(l, l1, nd) {
		kill_node(nd);
	}
}

static p_node
copy_nodelist(l)
	p_node	l;
{
	p_node	cp;
	p_node	l1;
	p_node	nd;

	node_initlist(&cp);
	node_walklist(l, l1, nd) {
		node_enlist(&cp, node_copy(nd));
	}
	return cp;
}

void
kill_node(nd)
	p_node	nd;
{
	/*	Put nodes that are no longer needed back onto the free
		list
	*/
	if (nd) {
		switch(nd->nd_class) {
		case Call:
			kill_node(nd->nd_callee);
			kill_node(nd->nd_obj);
			kill_nodelist(nd->nd_parlist);
			break;
		case Stat:
			kill_node(nd->nd_expr);
			kill_node(nd->nd_desig);
			kill_nodelist(nd->nd_list1);
			kill_nodelist(nd->nd_list2);
			break;
		case Aggr:
		case Row:
			kill_nodelist(nd->nd_memlist);
			break;
		case Def:
		case Name:
		case Value:
		case Tmp:
			break;
		default:
			if (nd->nd_symb) {
				kill_node(nd->nd_left);
				kill_node(nd->nd_right);
				nd->nd_left = 0;
				nd->nd_right = 0;
			}
			break;
		}

		/* Not as long as they are not copied from constants.
		switch(nd->nd_symb) {
		case REAL:
			if (nd->nd_real->r_real) free(nd->nd_real->r_real);
			free((char *) nd->nd_real);
			break;
		case STRING:
			if (nd->nd_string) free(nd->nd_string);
			free((char *) nd->nd_str);
			break;
		}
		*/
		free_node(nd);
	}
}

p_node
node_copy(nd)
	p_node	nd;
{
	/*	Put nodes that are no longer needed back onto the free
		list
	*/
	p_node	cp;

	if (! nd) return 0;
	cp = new_node();
	*cp = *nd;
	cp->nd_next = 0;
	cp->nd_prev = 0;
	switch(nd->nd_class) {
	case Call:
		cp->nd_obj = node_copy(nd->nd_obj);
		cp->nd_callee = node_copy(nd->nd_callee);
		cp->nd_parlist = copy_nodelist(nd->nd_parlist);
		break;
	case Stat:
		cp->nd_desig = node_copy(nd->nd_desig);
		cp->nd_expr = node_copy(nd->nd_expr);
		cp->nd_list1 = copy_nodelist(nd->nd_list1);
		cp->nd_list2 = copy_nodelist(nd->nd_list2);
		break;
	case Aggr:
	case Row:
		cp->nd_memlist = copy_nodelist(nd->nd_memlist);
		break;
	case Def:
	case Name:
	case Value:
	case Tmp:
		break;
	default:
		assert(nd->nd_left != nd);
		cp->nd_left = node_copy(nd->nd_left);
		cp->nd_right = node_copy(nd->nd_right);
		break;
	}
	return cp;
}

p_node
select_base(nodelist)
	p_node	nodelist;
{
	/*	Find the "root" of a selection chain.
	*/

	p_node	p;

	if (nodelist == 0) return 0;
	p = nodelist;
	for (;;) {
		switch(p->nd_class) {
		case Select:
		case Arrsel:
			p = p->nd_left;
			continue;
		}
		break;
	}
	return p;
}

#ifdef DEBUG

_PROTOTYPE(static void indnt, (int));
_PROTOTYPE(static void printnode, (p_node, int));
_PROTOTYPE(static char *nd_names, (int));

static char *
nd_names(v)
	int	v;
{
	switch(v) {
	case Value:
		return "Value";
	case Arrsel:
		return "ArrSel";
	case Oper:
		return "Oper";
	case Uoper:
		return "Uoper";
	case Call:
		return "Call";
	case Name:
		return "Name";
	case Aggr:
		return "Aggr";
	case Row:
		return "Row";
	case Def:
		return "Def";
	case Tmp:
		return "Tmp";
	case Stat:
		return "Stat";
	case Link:
		return "Link";
	case Select:
		return "Select";
	case Check:
		return "Check";
	case Ofldsel:
		return "Ofldsel";
	}
	return "Funny node";
}

static void
indnt(lvl)
	int	lvl;
{
	while (lvl--) {
		fputs("  ", stdout);
	}
}

static void
printnode(nd, lvl)
	p_node	nd;
	int	lvl;
{
	indnt(lvl);
	printf("%s(%s, %d, flgs=0x%x, num=%d)",
		nd_names(nd->nd_class),
		nd->nd_symb == 0 ? "0" : symbol2str(nd->nd_symb),
		nd->nd_lineno,
		nd->nd_flags,
		nd->nd_nodenum);
	switch(nd->nd_class) {
	case Def:
		(void) dump_def(nd->nd_def);
		break;
	case Name:
		printf(" %s\n", nd->nd_idf->id_text);
		break;
	case Tmp:
		if (nd->nd_flags & ND_RETVAR) {
			printf(" return variable\n");
		}
		else	printf(" %d\n", nd->nd_tmpvar->tmp_id);
		break;
	case Stat:
		if (nd->nd_o_info) {
			printf(" extra: %d, %d\n",
				nd->nd_o_info->o_ndnum[0],
				nd->nd_o_info->o_ndnum[1]);
			break;
		}
		/* Fall through */
	default:
		printf("\n");
		break;
	}
	if (nd->nd_class != Stat && nd->nd_type) {
		indnt(lvl);
		printf(" Type: ");
		(void) dump_type(nd->nd_type, 1);
		printf("\n");
	}
}

int
dump_nodelist(l, lvl)
	p_node	l;
	int	lvl;
{
	p_node	l1;
	p_node	nd;

	node_walklist(l, l1, nd) {
		(void) dump_node(nd, lvl);
	}
	return 0;
}

int
dump_node(nd, lvl)
	p_node	nd;
	int	lvl;
{
	if (! nd) {
		indnt(lvl); printf("<nilnode>\n");
		return 0;
	}
	printnode(nd, lvl);
	switch(nd->nd_class) {
	case Stat:
		if (nd->nd_desig) (void) dump_node(nd->nd_desig, lvl+1);
		if (nd->nd_expr) (void) dump_node(nd->nd_expr, lvl+1);
		if (! node_emptylist(nd->nd_list1)) {
			(void) dump_nodelist(nd->nd_list1, lvl+1);
		}
		if (! node_emptylist(nd->nd_list2)) {
			(void) dump_nodelist(nd->nd_list2, lvl+1);
		}
		break;
	case Aggr:
	case Row:
		(void) dump_nodelist(nd->nd_memlist, lvl);
		break;
	case Link:
		if (nd->nd_left) (void) dump_node(nd->nd_left, lvl+1);
		if (nd->nd_right) (void) dump_node(nd->nd_right, lvl+1);
		break;
	case Arrsel:
	case Oper:
	case Select:
	case Check:
	case Ofldsel:
		(void) dump_node(nd->nd_left, lvl+1);
		/* Fall through */
	case Uoper:
		(void) dump_node(nd->nd_right, lvl+1);
		break;
	case Call:
		(void) dump_node(nd->nd_callee, lvl+1);
		(void) dump_node(nd->nd_obj, lvl+1);
		(void) dump_nodelist(nd->nd_parlist, lvl+1);
		break;
	}
	return 0;
}
#endif /* DEBUG */
