/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* D A T A   F L O W */

/* $Id: data_flow.c,v 1.5 1997/05/15 12:01:47 ceriel Exp $ */

#include	"debug.h"
#include	"ansi.h"

#include	<alloc.h>
#include	<assert.h>
#include	<stdio.h>

/* #include	"LLlex.h" */
/* #include	"node.h" */
#include	"sets.h"
#include	"data_flow.h"

_PROTOTYPE(static void dfl_compute_links, (dfl_graph grph));
_PROTOTYPE(static void dfl_compute_reachable, (dfl_graph grph));
_PROTOTYPE(static int forward_flow,
		(dfl_graph grph, int (*meet)(), int (*transfer)()));
_PROTOTYPE(static int backward_flow,
		(dfl_graph grph, int (*meet)(), int (*transfer)()));


/* A flow graph is represented by the following structure: */

struct fl_graph {
	int	fl_nnodes;
	int	fl_setsize;
	int	*fl_nsuccessors;
	int	*fl_npredecessors;
	p_set	*fl_successors;
	p_set	*fl_predecessors;
	int	*fl_links;
	int	*fl_predec_index;
	int	*fl_succ_index;
	p_set	fl_reachable;
};

/* The fields of the graph contain the following:
	fl_nnodes:	the number of nodes in the graph.
	fl_setsize:	the size of a set of nodes.
	fl_nsuccessors:	for each node the number of successors it has.
	fl_npredecessors:
			for each node the number of predecessors it has.
	fl_successors:	for each node the set of successors.
	fl_predecessors:
			for each node the set of predecessors.
	fl_predec_index:
			for each node an index into fl_links which,
			when followed upwards, gives a -1 terminated
			list of predecessors.
	fl_succ_index:	for each node an index into fl_links which,
			when followed upwards, gives a -1 terminated
			list of successors.
	fl_links:	the array containing the -1 terminated lists.
	fl_reachable:	the set of nodes that are reachable from the
			starting node.
*/

dfl_graph dfl_init_graph(nnodes)
	int	nnodes;
{
	/* Allocate and initialize everything but the fl_links field.
	   We don't know yet how large the fl_links field should be
	   because its size depends on the number of links.
	*/

	dfl_graph
		grph = (dfl_graph) Malloc(sizeof(struct fl_graph));
	int	i;

	grph->fl_nnodes = nnodes;
	grph->fl_successors = (p_set *) Malloc(nnodes * sizeof(p_set));
	grph->fl_predecessors = (p_set *) Malloc(nnodes * sizeof(p_set));
	grph->fl_setsize = set_size(nnodes);
	grph->fl_nsuccessors = (int *) Malloc(nnodes * sizeof(int));
	grph->fl_npredecessors = (int *) Malloc(nnodes * sizeof(int));
	grph->fl_predec_index = (int *) Malloc(nnodes * sizeof(int));
	grph->fl_succ_index = (int *) Malloc(nnodes * sizeof(int));
	grph->fl_links = 0;
	grph->fl_reachable = set_create(grph->fl_setsize);
	set_init(grph->fl_reachable, 0, grph->fl_setsize);
	for (i = nnodes-1; i >= 0; i--) {
		grph->fl_successors[i] = set_create(grph->fl_setsize);
		set_init(grph->fl_successors[i], 0, grph->fl_setsize);
		grph->fl_predecessors[i] = set_create(grph->fl_setsize);
		set_init(grph->fl_predecessors[i], 0, grph->fl_setsize);
		grph->fl_nsuccessors[i] = 0;
		grph->fl_npredecessors[i] = 0;
	}
	return grph;
}

void dfl_freegraph(grph)
	dfl_graph
		grph;
{
	/* Remove a graph. */

	int	i;

	for (i = grph->fl_nnodes - 1; i >= 0; i--) {
		set_free(grph->fl_successors[i], grph->fl_setsize);
		set_free(grph->fl_predecessors[i], grph->fl_setsize);
	}
	free(grph->fl_successors);
	free(grph->fl_nsuccessors);
	free(grph->fl_predecessors);
	free(grph->fl_npredecessors);
	free(grph->fl_predec_index);
	free(grph->fl_succ_index);
	set_free(grph->fl_reachable, grph->fl_setsize);
	if (grph->fl_links) free(grph->fl_links);
	free(grph);
}

void dfl_addlink(grph, src, dst)
	dfl_graph
		grph;
	int	src, dst;
{
	/* Add a link from src to dst in graph grph. */

	/* The next two assertions are short-hands for the test
		0 <= <arg> < grph->fl_nnodes,
	   where <arg> is either src or dst.
	*/
	assert((unsigned int) src < grph->fl_nnodes);
	assert((unsigned int) dst < grph->fl_nnodes);

	grph->fl_nsuccessors[src]++;
	set_putmember(grph->fl_successors[src], dst);
	grph->fl_npredecessors[dst]++;
	set_putmember(grph->fl_predecessors[dst], src);
}

void dfl_graphcomplete(grph)
	dfl_graph
		grph;
{
	dfl_compute_reachable(grph);
	dfl_compute_links(grph);
}

static void dfl_compute_reachable(grph)
	dfl_graph
		grph;
{
	/* Compute the set of reachable nodes, starting from the 0 node.  */

	int	change = 1;
	int	i, j;

	set_putmember(grph->fl_reachable, 0);
	while (change) {
		change = 0;
		for (i = 0; i < grph->fl_nnodes; i++) {
			if (set_ismember(grph->fl_reachable, i)) {
				change |= set_union(grph->fl_reachable,
						    grph->fl_reachable,
						    grph->fl_successors[i],
						    grph->fl_setsize);
			}
		}
	}

	/* Also clear all links in unreachable nodes. */
	for (i = 0; i < grph->fl_nnodes; i++) {
		if (! set_ismember(grph->fl_reachable, i)) {
			p_set tmp = grph->fl_successors[i];
			for (j = 0; j < grph->fl_nnodes; j++) {
				if (set_ismember(tmp, j)) {
				    set_clrmember(grph->fl_predecessors[j], i);
				}
			}
			tmp = grph->fl_predecessors[i];
			for (j = 0; j < grph->fl_nnodes; j++) {
				if (set_ismember(tmp, j)) {
				    set_clrmember(grph->fl_successors[j], i);
				}
			}
		}
	}
}

static void dfl_compute_links(grph)
	dfl_graph
		grph;
{
	/* Compute the fl_links representation of the graph.  */

	int	link_count = 0;
	int	i, j;
	int	indx = 1;

	if (grph->fl_links) return;

	for (i = 0; i < grph->fl_nnodes; i++) {
		link_count += grph->fl_nsuccessors[i];
	}
	link_count += grph->fl_nnodes;
	link_count <<= 1;
	link_count++;
	grph->fl_links = (int *) Malloc(link_count * sizeof(int));
	grph->fl_links[0] = -1;
	for (i = 0; i < grph->fl_nnodes; i++) {
		grph->fl_succ_index[i] = indx;
		for (j = 0; j < grph->fl_nnodes; j++) {
			if (set_ismember(grph->fl_successors[i], j)) {
				grph->fl_links[indx++] = j;
			}
		}
		grph->fl_links[indx++] = -1;
		grph->fl_predec_index[i] = indx;
		for (j = grph->fl_nnodes - 1; j >= 0; j--) {
			if (set_ismember(grph->fl_predecessors[i], j)) {
				grph->fl_links[indx++] = j;
			}
		}
		grph->fl_links[indx++] = -1;
	}
	assert(indx <= link_count);
}

int dfl_issuccessor(grph, src, dst)
	dfl_graph
		grph;
	int	src, dst;
{
	return set_ismember(grph->fl_successors[src], dst);
}

int dfl_ispredecessor(grph, src, dst)
	dfl_graph
		grph;
	int	src, dst;
{
	return set_ismember(grph->fl_predecessors[src], dst);
}

static int
forward_flow(grph, meet, transfer)
	dfl_graph
		grph;
	int	(*meet)();
	int	(*transfer)();
{
	int	i;
	int	change = 0;

	for (i = 0; i < grph->fl_nnodes; i++) {
		int *p;
		if (grph->fl_npredecessors[i] > 1) {
			p = &(grph->fl_links[grph->fl_predec_index[i]]);
			change |= (*meet)(i, p);
		}
		if (grph->fl_nsuccessors[i] > 0) {
			p = &(grph->fl_links[grph->fl_succ_index[i]]);
			while (*p != -1) {
				if (grph->fl_npredecessors[*p] == 1) {
					change |= (*transfer)(i, *p);
				}
				p++;
			}
		}
	}
	return change;
}

void dfl_forward_flow(grph, meet, transfer)
	dfl_graph
		grph;
	int	(*meet)();
	int	(*transfer)();
{
#ifdef DEBUG
	int	count = 1;

	while (forward_flow(grph, meet, transfer)) {
		count++;
	}
	/* if (options['o']) printf("forward flow: %d iterations\n", count); */
#else
	while (forward_flow(grph, meet, transfer)) { }
#endif
}

static int
backward_flow(grph, meet, transfer)
	dfl_graph
		grph;
	int	(*meet)();
	int	(*transfer)();
{
	int	i;
	int	change = 0;

	for (i = grph->fl_nnodes - 1; i >= 0; i--) {
		int *p;
		if (grph->fl_nsuccessors[i] > 1) {
			p = &(grph->fl_links[grph->fl_succ_index[i]]);
			change |= (*meet)(i, p);
		}
		if (grph->fl_npredecessors[i] > 0) {
			p = &(grph->fl_links[grph->fl_predec_index[i]]);
			while (*p != -1) {
				if (grph->fl_nsuccessors[*p] == 1) {
					change |= (*transfer)(i, *p);
				}
				p++;
			}
		}
	}
	return change;
}

void dfl_backward_flow(grph, meet, transfer)
	dfl_graph
		grph;
	int	(*meet)();
	int	(*transfer)();
{
#ifdef DEBUG
	int	count = 1;

	while (backward_flow(grph, meet, transfer)) {
		count++;
	}
	/* if (options['o']) printf("backward flow: %d iterations\n", count); */
#else
	while (backward_flow(grph, meet, transfer)) { }
#endif
}

#ifdef DEBUG
void dfl_printgraph(grph)
	dfl_graph
		grph;
{
	int	i;

	for (i = 0; i < grph->fl_nnodes; i++) {
		int *p;

		printf("Node %d: Predecessors: ", i);
		if (grph->fl_npredecessors[i] > 0) {
			p = &(grph->fl_links[grph->fl_predec_index[i]]);
			while (*p != -1) {
				printf("%d ", *p);
				p++;
			}
		}
		printf("; Successors: ");
		if (grph->fl_nsuccessors[i] > 0) {
			p = &(grph->fl_links[grph->fl_succ_index[i]]);
			while (*p != -1) {
				printf("%d ", *p);
				p++;
			}
		}
		printf(";\n");
	}
}
#endif
