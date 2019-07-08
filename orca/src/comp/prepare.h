/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __PREPARE_H__
#define __PREPARE_H__

/*   P R E P A R A T I O N   */

/* $Id: prepare.h,v 1.4 1997/05/15 12:02:48 ceriel Exp $ */

#include	"ansi.h"
#include	"def.h"

/* This file contains routines to prepare the parse tree for optimization and
   code generation. The AND and OR operators are rewritten as ANDBECOMES and
   ORBECOMES, and set operations are rewritten as well.
*/

_PROTOTYPE(void prepare_df, (p_def df));
	/*	Prepares the body of operation/function/process/object df.
	*/

#endif /* __PREPARE_H__ */
