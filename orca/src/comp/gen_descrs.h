/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __GEN_DESCRS_H__
#define __GEN_DESCRS_H__

/* $Id: gen_descrs.h,v 1.9 1997/05/15 12:02:11 ceriel Exp $ */

#include	"ansi.h"
#include	"def.h"

_PROTOTYPE(void generate_descrs, (p_def df));
	/*	Generate runtime descriptors and typedefs for the type/object/
		module/process/variable 'df'.
	*/

_PROTOTYPE(void gen_localdescrs, (p_def df));
	/*	Generate runtime descriptors and typedefs for types and
		variables declared locally within function/process/operation
		'df'.
	*/

#endif /* __GEN_DESCRS_H__ */
