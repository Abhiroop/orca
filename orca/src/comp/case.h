/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __CASE_H__
#define __CASE_H__

/* $Id: case.h,v 1.7 1997/05/15 12:01:36 ceriel Exp $ */

#include	"ansi.h"
#include	"node.h"

/*   A N A L Y S I S   O F   C A S E   S T A T E M E N T S   */

_PROTOTYPE(p_node case_analyze,
	   (p_node expr, p_node list, p_node elsepart, int has_elsepart));
	/*	Analyzes case labels of a case statement: no label may occur
		more than once. Entries with large ranges are put in front,
		so that they can be compiled with an if-statement.
		The case-expression is indicated by 'expr'. The cases are
		indicated by 'list'. 'has_elsepart' indicates wether there was
		an else part, end 'elsepart' indicates the else part. Note that
		elsepart may be NULL, even when has_elsepart is set. In this
		case, there was an empty else part, which has different
		semantics from no else part at all.
		The resulting case-statement tree is returned.
	*/

#endif /* __CASE_H__ */
