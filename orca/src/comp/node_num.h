/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __NODE_NUM_H__
#define __NODE_NUM_H__

/* N O D E   N U M B E R I N G */

/* $Id: node_num.h,v 1.6 1997/05/15 12:02:34 ceriel Exp $ */

#include	"ansi.h"
#include	"node.h"

_PROTOTYPE(int number_statements, (p_node list, p_node **stats));
	/*	Numbers the statements in statement list 'list' (including
		nested statements), and builds a table of statements which
		can be indexed with the statement number.
		Returns the total number of statements.
	*/

_PROTOTYPE(int number_expressions, (p_node list, p_node **exprs));
	/*	Numbers the expressions in expression list 'list' (including
		nested expressions), and builds a table of expressions which
		can be indexed with the expression number.
		Returns the total number of expressions.
	*/

#endif /* __NODE_NUM_H__ */
