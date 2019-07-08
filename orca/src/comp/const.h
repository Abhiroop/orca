/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __CONST_H__
#define __CONST_H__

/* C O N S T A N T   E X P R E S S I O N   H A N D L I N G */

/* $Id: const.h,v 1.7 1997/05/15 12:01:46 ceriel Exp $ */

#include	"ansi.h"
#include	"node.h"

extern long	full_mask[];	/* Indexed by size.
				   full_mask[1] == 0xFF, full_mask[2] == 0xFFFF,
				   ...
				*/
extern long	max_int[];	/* Indexed by size.
				   max_int[1] == 0x7F, max_int[2] == 0x7FFF,
				   ...
				*/
extern long	min_int[];	/* Indexed by size.
				   min_int[1] == 0xFFFFFF80,
				   min_int[2] == 0xFFFF8000,
				   ...
				*/

_PROTOTYPE(void init_cst, (void));
	/*	Initializes the variables described above.
	*/

_PROTOTYPE(p_node cst_unary, (p_node nd));
	/*	The unary operation in 'nd' is performed on the constant
		expression below it, and the result returned.
		This version is for INTEGER expressions.
	*/

_PROTOTYPE(p_node cst_relational, (p_node nd));
	/*	The relational expression in 'nd' is performed on the constants
		below it, and the result returned.
		This version is for INTEGER expressions.
	*/

_PROTOTYPE(p_node cst_arithop, (p_node nd));
	/*	The binary operator in 'nd' is performed on the constant
		expressions below it, and the result returned.
		This version is for INTEGER expressions.
	*/

_PROTOTYPE(p_node cst_boolop, (p_node nd));
	/*	The boolean operator in 'nd' is performed. One or more of the
		operands are constant. The resulting expression is returned.
	*/

_PROTOTYPE(p_node cst_call, (p_node nd, int builtin));
	/*	A call of a built-in is found that can be evaluated compile-
		time. The result is returned.
		This version is for INTEGER expressions.
	*/

_PROTOTYPE(p_node fcst_unary, (p_node bd));
	/*	The unary operation in 'nd' is performed on the constant
		expression below it, and the result returned.
		This version is for REAL expressions.
	*/

_PROTOTYPE(p_node fcst_relational, (p_node nd));
	/*	The relational expression in 'nd' is performed on the constants
		below it, and the result returned.
		This version is for REAL expressions.
	*/

_PROTOTYPE(p_node fcst_arithop, (p_node nd));
	/*	The binary operator in 'nd' is performed on the constant
		expressions below it, and the result returned.
		This version is for REAL expressions.
	*/

_PROTOTYPE(p_node fcst_call, (p_node nd, int builtin));
	/*	A call of a built-in is found that can be evaluated compile-
		time. The result is returned.
		This version is for REAL expressions.
	*/

_PROTOTYPE(int get_wordsize, (void));
	/*	Gets the current size (in bytes) in which integer expressions
		are computed.
	*/

_PROTOTYPE(void set_wordsize, (int new_wordsize));
	/*	Sets the size (in bytes) in which integer expressions are
		computed. The new wordsize must be <= sizeof(long).
	*/
#endif /* __CONST_H__ */
