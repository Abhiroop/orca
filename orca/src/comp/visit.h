/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __VISIT_H__
#define __VISIT_H__

/* $Id: visit.h,v 1.8 1997/05/15 12:03:24 ceriel Exp $ */

#include	"ansi.h"
#include	"node.h"

/* Tree walker. */

/* These routines walk through the tree/list indicated by their first
   argument, and appy 'func' to each node that has an 'nd_class' field
   which is a member of the bitset indicated by 'kinds'.
   If 'bottom_up' is set, sub-trees are walked first, before 'func'
   is called.
   If 'func' returns non-zero, the sub-tree below it is skipped
   (obviously only possible when 'bottom-up' is not set).
*/

_PROTOTYPE(void visit_node,
	   (p_node nd, int kinds, int (*func)(p_node), int bottom_up));
	/*	Walks through a tree.
	*/

_PROTOTYPE(void visit_ndlist,
	   (p_node nd, int kinds, int (*func)(p_node), int bottom_up));
	/*	Walks through a list, calling visit_node for each element.
	*/

#endif /* __VISIT_H__ */
