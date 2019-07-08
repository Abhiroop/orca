/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __DATA_FLOW_H__
#define __DATA_FLOW_H__

/* D A T A   F L O W */

/* $Id: data_flow.h,v 1.5 1997/05/15 12:01:48 ceriel Exp $ */

#include	"ansi.h"
#include	"debug.h"

typedef struct fl_graph
	*dfl_graph;

/* The data flow framework is based on a graph. The graph is built by the
   user, using the following calls:

       dfl_graph dfl_init_graph(int nnodes);
       void dfl_addlink(dfl_graph graph, int src, int dst);
       void dfl_graphcomplete(dfl_graph graph);

   Note that nodes should be numbered from 0 to nnodes-1.
   Node 0 is assumed to be the starting node, and node nnodes-1 is assumed
   to be the end node.
*/

_PROTOTYPE(dfl_graph dfl_init_graph, (int nnodes));
	/*	Creates a graph datastructure for 'nnodes' nodes and returns
		a pointer to it.
	*/

_PROTOTYPE(void dfl_addlink, (dfl_graph graph, int src, int dst));
	/*	Adds a link in the graph from src to dst (so that dst is
		a successor of src and src is a predecessor of dst.
	*/

_PROTOTYPE(void dfl_graphcomplete, (dfl_graph graph));
	/*	When all links are added, this routine should be called.
		It completes the internal administration of the graph.
	*/

/* To dispose of a graph, use the following routine. */
_PROTOTYPE(void dfl_freegraph, (dfl_graph graph));
	/*	Disposes (frees) the graph indicated by 'graph'.
	*/

_PROTOTYPE(int dfl_issuccessor, (dfl_graph graph, int src, int dst));
	/*	Inquiry routine: is dst a successor of src in graph 'graph'?
	*/

_PROTOTYPE(int dfl_ispredecessor, (dfl_graph graph, int src, int dst));
	/*	Inquiry routine: is dst a predecessor of src in graph 'graph'?
	*/

/* The data flow framework itself has two entry points: dfl_forward_flow
   and dfl_backward_flow, for forward and backward problems respectively.

   The mechanism itself knows nothing about sets. Instead,
   it deals with integers, which indicate nodes in a graph.

   The user has to supply a MEET routine and a TRANSFER routine.

   The MEET routine usually involves set-intersection or set-union. It has the
   following interface:

	int MEET(int destination, int *nodelist)

   and is supposed to put the MEET of the sets indicated by nodelist (which is
   a pointer to a -1-terminated list array of integers) in the destination set,
   and return 1 if the destination set changed, and 0 otherwise.

   The TRANSFER routine has to implement the effect of a transfer.
   It has the following interface:

	int TRANSFER(int source, int destination)

   and is supposed to put the effect of the transfer on the set indicated by
   source into the set indicated by destination, and return 1 if it changed, 0
   if it did not.
*/

_PROTOTYPE(void dfl_forward_flow,
		(dfl_graph graph, int (*meet)(), int (*transfer)()));
	/*	Solves a forward dataflow problem, using flow graph 'graph',
		meet-function 'meet' and transfer function 'transfer'.
	*/

_PROTOTYPE(void dfl_backward_flow,
		(dfl_graph graph, int (*meet)(), int (*transfer)()));
	/*	Solves a backward dataflow problem, using flow graph 'graph',
		meet-function 'meet' and transfer function 'transfer'.
	*/

#ifdef DEBUG
_PROTOTYPE(void dfl_printgraph, (dfl_graph grph));
	/*	For each node in graph 'grph', print a line containing the
		node number, its successors, and its predecessors.
	*/
#endif
#endif /* __DATA_FLOW_H__ */
