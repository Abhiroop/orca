/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* B U I L D   D A T A	 F L O W   G R A P H */

/* $Id: bld_graph.c,v 1.5 1997/05/15 12:01:34 ceriel Exp $ */

#include	"ansi.h"
#include	"debug.h"

#include	"bld_graph.h"

_PROTOTYPE(static int build_graph, (p_node, dfl_graph, int, int));

static int	endstatno;

dfl_graph
dfl_buildgraph(nstates, beginstate, statlist)
	int	nstates;
	int	beginstate;
	p_node	statlist;
{
	dfl_graph
		grph = dfl_init_graph(nstates);

	/* Add a link from the begin state to the first statement of the
	   list.
	*/
	dfl_addlink(grph, beginstate, node_getlistel(statlist)->nd_nodenum);
	endstatno = nstates-1;
	(void) build_graph(statlist, grph, 0, endstatno);
	dfl_graphcomplete(grph);
	return grph;
}

static int
build_graph(l, graph, exit, next)
	p_node	l;
	dfl_graph
		graph;
	int	exit, next;
{
	/* Compute successors and predecessors for the statements in
	   statement list l. exit indicates the target of a possible
	   EXIT. next indicates the number following l.
	   Return wether the 'next' state is reachable.
	*/
	p_node	nd;
	static int
		level;
	int	reaching = 1;
	int	tmp;

	level++;
	node_walklist(l, l, nd) {
	    p_node
		nd1,
		nxt = node_getlistel(l);
	    int	num, num2;
	    p_node
		l2;

	    switch(nd->nd_symb) {
	    case IF:
		/* num indicates the place after the IF-expression, so
		   it is a successor of nd->nd_nodenum and a predecessor
		   of the head of the IF-part as well as the ELSE-part.
		*/
		num = nd->nd_o_info->o_ndnum[0];
		dfl_addlink(graph, nd->nd_nodenum, num);
		dfl_addlink(graph,
			    num,
			    node_getlistel(nd->nd_list1)->nd_nodenum);
		tmp = build_graph(nd->nd_list1, graph, exit, nxt->nd_nodenum);
		if (nd->nd_list2) {
		    dfl_addlink(graph,
				num,
				node_getlistel(nd->nd_list2)->nd_nodenum);
		    tmp |= build_graph(	nd->nd_list2,
					graph,
					exit,
					nxt->nd_nodenum);
		    if (! tmp) reaching = 0;
		}
		else {
		    dfl_addlink(graph, num, nxt->nd_nodenum);
		}
		break;

	    case CASE:
		/* num indicates the place after the CASE-expression.
		*/
		num = nd->nd_o_info->o_ndnum[0];
		dfl_addlink(graph, nd->nd_nodenum, num);
		tmp = 0;
		node_walklist(nd->nd_list1, l2, nd1) {
		    dfl_addlink(graph,
				num,
				node_getlistel(nd1->nd_list2)->nd_nodenum);
		    tmp |= build_graph(	nd1->nd_list2,
					graph,
					exit,
					nxt->nd_nodenum);
		}
		if (nd->nd_list2) {
		    dfl_addlink(graph,
				num,
				node_getlistel(nd->nd_list2)->nd_nodenum);
		    tmp |= build_graph(	nd->nd_list2,
					graph,
					exit,
					nxt->nd_nodenum);
		}
		if (! tmp) reaching = 0;
		break;

	    case DO:
		nd1 = node_getlistel(node_prevlistel(nd->nd_list1));
		dfl_addlink(
			graph,
			nd->nd_nodenum,
			node_getlistel(nd->nd_list1)->nd_nodenum);
		(void) build_graph(
			nd->nd_list1,
			graph,
			nxt->nd_nodenum,
			node_getlistel(nd->nd_list1)->nd_nodenum);
		break;

	    case FOR:
		/* num indicates the position after the FOR-loop
		   initialization, num2 indicates the position right
		   in front of the loop.
		*/
		num = nd->nd_o_info->o_ndnum[0];
		if (node_emptylist(nd->nd_list2)) {
			num2 = nd->nd_o_info->o_ndnum[1];
			dfl_addlink(graph,
				    num2,
				    node_getlistel(nd->nd_list1)->nd_nodenum);
		}
		else	{
			num2 = node_getlistel(nd->nd_list2)->nd_nodenum;
			build_graph(nd->nd_list2,
				    graph,
				    exit,
				    node_getlistel(nd->nd_list1)->nd_nodenum);
		}
		dfl_addlink(graph, nd->nd_nodenum, num);
		dfl_addlink(graph, num, num2);
		/* Check if the FOR-loop is executed at least once. If so,
		   there is no link from num to nxt->nd_nodenum.
		*/
		if (! (nd->nd_flags & ND_FORDONE)) {
			dfl_addlink(graph, num, nxt->nd_nodenum);
		}
		if (build_graph(nd->nd_list1,
				graph,
				nxt->nd_nodenum,
				nxt->nd_nodenum)) {
		    nd1 = node_getlistel(node_prevlistel(nd->nd_list1));
		    dfl_addlink(graph, nd1->nd_nodenum,
			    node_getlistel(nd->nd_list1)->nd_nodenum);
		}
		break;

	    case COND_EXIT:
		/* num indicates the position after the expression. */
		num = nd->nd_o_info->o_ndnum[0];
		dfl_addlink(graph, nd->nd_nodenum, num);
		dfl_addlink(graph, num, nxt->nd_nodenum);
		dfl_addlink(graph, num, exit);
		break;

	    case FOR_UPDATE:
	    case UPDATE:
		dfl_addlink(graph, nd->nd_nodenum, nxt->nd_nodenum);
		if (nxt->nd_symb == 0) {
			dfl_addlink(graph, nd->nd_nodenum, next);
			return reaching;
		}
		break;

	    case GUARD:
		dfl_addlink(graph, 0, nd->nd_nodenum);
		num = nd->nd_o_info->o_ndnum[0];
		dfl_addlink(graph, nd->nd_nodenum, num);
		dfl_addlink(graph,
			    num,
			    node_getlistel(nd->nd_list1)->nd_nodenum);
		(void) build_graph(nd->nd_list1, graph, exit, endstatno);
		break;

	    case ARROW:
		build_graph(nd->nd_list2, graph, exit, next);
		break;

	    case EXIT:
		dfl_addlink(graph, nd->nd_nodenum, exit);
		reaching = 0;
		break;

	    case RETURN:
		if (nd->nd_expr) {
		    num = nd->nd_o_info->o_ndnum[0];
		    dfl_addlink(graph, nd->nd_nodenum, num);
		    dfl_addlink(graph, num, endstatno);
		}
		else dfl_addlink(graph, nd->nd_nodenum, endstatno);
		reaching = 0;
		break;

	    case ORBECOMES:
	    case ANDBECOMES:
		if (nxt) {
			dfl_addlink(graph, nd->nd_nodenum, nxt->nd_nodenum);
		}
		if (! node_emptylist(nd->nd_list1)) {
			dfl_addlink(graph,
				    nd->nd_nodenum,
				    node_getlistel(nd->nd_list1)->nd_nodenum);
			build_graph(nd->nd_list1, graph, exit,
				   nxt ? nxt->nd_nodenum : -1);
		}
		break;
	    case ',':
		/* special case, added for LV analysis. */
		if (! node_emptylist(nd->nd_list1)) {
			dfl_addlink(graph, nd->nd_nodenum, node_getlistel(nd->nd_list1)->nd_nodenum);
			build_graph(nd->nd_list1, graph, exit, next);
		}
		if (! node_emptylist(nd->nd_list2)) {
			dfl_addlink(graph, nd->nd_nodenum, node_getlistel(nd->nd_list2)->nd_nodenum);
			build_graph(nd->nd_list2, graph, exit, next);
		}
		break;

	    case 0:
		if (! nxt && next >= 0) {
			dfl_addlink(graph, nd->nd_nodenum, next);
			break;
		}
		dfl_addlink(graph, nd->nd_nodenum, nxt->nd_nodenum);
		break;

	    default:
		if (nxt) {
		    dfl_addlink(graph, nd->nd_nodenum, nxt->nd_nodenum);
		}
		break;
	    }
	    if (! reaching) {
		level--;
		return 0;
	    }
	}
	level--;
	return reaching;
}
