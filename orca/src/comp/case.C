/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: case.C,v 1.12 1997/05/15 12:01:35 ceriel Exp $ */

#include	"debug.h"
#include	"ansi.h"

#include	<stdio.h>
#include	<alloc.h>
#include	<assert.h>

#include	"case.h"
#include	"LLlex.h"
#include	"error.h"
#include	"def.h"
#include	"chk.h"

_PROTOTYPE(static void add_cases, (p_node));
_PROTOTYPE(static void add_one_case, (p_node, p_node));

/* List of case table entries. */
static struct case_entry	{
	struct case_entry *ce_next;	/* next in list */
	long ce_low, ce_up;		/* lower and upper bound of range */
} *entries;

/* STATICALLOCDEF "case_entry" 20 */

#define CASE_THRESHOLD 100

p_node
case_analyze(expr, ndl, ep, has_else)
	p_node	expr;
	p_node	ndl;
	p_node	ep;
	int	has_else;
{
	/*	Analyze case labels of a case statement: check that no
		label occurs more than once, and put the entries with
		large ranges in front, so that they can be compiled with
		an if-statement.
	*/

	p_node	retval = mk_leaf(Stat, CASE);
	p_node	l,
		nwlist;
	p_node	nd;
	p_node	currcase;
	struct case_entry
		*p;

	mark_defs(expr, D_USED);
	retval->nd_expr = expr;
	retval->nd_list2 = ep;
	if (has_else) retval->nd_flags |= ND_HAS_ELSEPART;
	node_initlist(&nwlist);
	node_walklist(ndl, l, currcase) {
		assert(currcase->nd_symb == ARROW);
		add_cases(currcase);
	}

	while ((p = entries)) {
		entries = p->ce_next;
		free_case_entry(p);
	}

	node_walklist(ndl, l, nd) {
		if (nd->nd_flags & ND_LARGE_RANGE) {
			node_fromlist(&ndl, nd);
			node_enlist(&nwlist, nd);
		}
	}
	if (! node_emptylist(nwlist)) {
		node_walklist(ndl, l, nd) {
			if (! (nd->nd_flags & ND_LARGE_RANGE)) {
				node_fromlist(&ndl, nd);
				node_enlist(&nwlist, nd);
			}
		}
		node_killlist(&ndl);
		ndl = nwlist;
	}
	retval->nd_list1 = ndl;
	return retval;
}

static void
add_cases(currcase)
	p_node	currcase;
{
	p_node	l;
	p_node	nd;

	node_walklist(currcase->nd_list1, l, nd) {
		add_one_case(currcase, nd);
	}
}

static void
add_one_case(currcase, nd)
	p_node	currcase;	/* Current case entry, indicates a list of
				   ranges and/or numbers.
				*/
	p_node	nd;		/* Indicates current range or number within
				   the list indicated by currcase.
				*/
{
	struct case_entry
		*ce = new_case_entry(),
		*c1 = entries,
		*c2 = 0;

	if (nd->nd_symb == UPTO) {
		ce->ce_low = nd->nd_left->nd_int;
		ce->ce_up = nd->nd_right->nd_int;

		if (ce->ce_low > ce->ce_up) {
			pos_error(&nd->nd_pos,
				"empty range in case label");
			free_case_entry(ce);
			return;
		}
		if (ce->ce_up - ce->ce_low > CASE_THRESHOLD) {
			/* current case entry to be translated using an
			   if-statement.
			*/
			currcase->nd_flags |= ND_LARGE_RANGE;
		}
	}
	else {
		ce->ce_low = nd->nd_int;
		ce->ce_up = nd->nd_int;
	}

	if (! entries) {
		entries = ce;
		return;
	}

	/* Insert the current case at the proper place in the list of entries.
	   The list is sorted, to detect multiple case entries.
	*/
	while (c1 && c1->ce_low < ce->ce_low) {
		c2 = c1;
		c1 = c1->ce_next;
	}
	/*	At this point three cases are possible:
		1: c1 != 0 && c2 != 0:
			insert ce somewhere in the middle
		2: c1 != 0 && c2 == 0:
			insert ce right after the head
		3: c1 == 0 && c2 != 0:
			append ce to last element
		The case c1 == 0 && c2 == 0 cannot occur, since
		the list is guaranteed not to be empty.
	*/
	if (c2) {
		if (c2->ce_up >= ce->ce_low) {
			pos_error(&nd->nd_pos,
				"multiple case entry for value %ld",
				ce->ce_low);
			free_case_entry(ce);
			return;
		}
	}
	if (c1)	{
		if (ce->ce_up >= c1->ce_low) {
			pos_error(&nd->nd_pos,
				"multiple case entry for value %ld",
				ce->ce_up);
			free_case_entry(ce);
			return;
		}
		if (c2)	{
			ce->ce_next = c2->ce_next;
			c2->ce_next = ce;
		}
		else	{
			ce->ce_next = entries;
			entries = ce;
		}
	}
	else	{
		assert(c2);

		c2->ce_next = ce;
	}
}
