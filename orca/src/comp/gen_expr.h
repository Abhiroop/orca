/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __GEN_EXPR_H__
#define __GEN_EXPR_H__

/* $Id: gen_expr.h,v 1.10 1997/05/15 12:02:13 ceriel Exp $ */

#include	<stdio.h>
#include	"ansi.h"
#include	"node.h"
#include	"temps.h"

/* Code generation for expressions. */

_PROTOTYPE(void gen_expr, (FILE *f, p_node nd));
	/*	Generates the expression indicated by 'nd' on file 'f'.
	*/

_PROTOTYPE(void gen_addr, (FILE *f, p_node nd));
	/*	Generates the address of the designator indicated by 'nd' on
		file 'f'.
	*/

_PROTOTYPE(void gen_numconst, (FILE *f, p_node nd));
	/*	Generates the numeric constant indicated by nd on file 'f'.
	*/

_PROTOTYPE(char *c_sym, (int operator));
	/*	Returns the C operator (as a string) that corresponds to the
		Orca operator.
	*/

extern int no_temporaries;

#endif /* __GEN_EXPR_H__ */
