/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __SIMPLIFY_H__
#define __SIMPLIFY_H__

/*   S I M P L I F I C A T I O N   A N D   T E M P O R A R I E S   */

/* $Id: simplify.h,v 1.8 1997/05/15 12:02:58 ceriel Exp $ */

#include	"ansi.h"
#include	"def.h"
#include	"node.h"

_PROTOTYPE(void simplify_df, (p_def df));
	/*	 Simplify and allocate temporaries for the
		function/operation/process indicated by 'df'.
	*/

_PROTOTYPE(int indicates_local_object, (p_node nd));
	/*	When 'nd' indicates an expression which contains an object,
		this routine can be called to decide if the object is local
		or shared. It is pessimistic, t.i., if it cannot decide,
		it assumes that the object is shared.
	*/

_PROTOTYPE(void mk_temp_expr, (p_node arg, int isptr, p_node *ndlist));
	/*	Create a temporary for the expression in 'arg'. Its
		initialization is added to the node list in 'ndlist'.
		Return a reference to the temporary. 'isptr' is set when
		the temporary is supposed to indicate the address of 'arg'.
	*/

_PROTOTYPE(void mk_designator, (p_node arg, p_node *ndlist));
	/*	Convert the expression in 'arg' to a designator, allocating
		temporaries when needed, and appending the temporaries
		initialization to 'ndlist'.
	*/

_PROTOTYPE(void mk_simple_desig, (p_node arg, p_node *ndlist));
	/*	Convert the expression in 'arg' to a simple one that can be
		designated. The required temporaries initializations are
		appended to 'ndlist'.
	*/

_PROTOTYPE(void mk_simple_expr, (p_node arg, p_node *ndlist));
	/*	Convert any expression to a simple one.
	*/

#endif /* __SIMPLIFY_H__ */
