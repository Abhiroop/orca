/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* B U I L D   D A T A	 F L O W   G R A P H */

/* $Id: bld_graph.h,v 1.4 1997/05/15 12:01:35 ceriel Exp $ */

#include	"ansi.h"
#include	"data_flow.h"
#include	"node.h"

_PROTOTYPE(dfl_graph dfl_buildgraph,
		(int nstates, int beginstate, p_node statlist));
	/*	Builds the flow graph for statement list statlist. The graph
		will have nstates graph nodes, and its beginstate will be
		'beginstate'. The other state numbers are indicated by the
		node numbering.
	*/
