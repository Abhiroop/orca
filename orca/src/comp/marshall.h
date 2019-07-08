/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __GEN_MARSHALL_H__
#define __GEN_MARSHALL_H__

/* $Id: marshall.h,v 1.4 1998/06/11 12:00:50 ceriel Exp $ */

#include	"ansi.h"
#include	"def.h"
#include	"type.h"

/* Marshalling routine generation. */

_PROTOTYPE(void gen_marshall, (p_def df));
	/*	Generate the marshalling functions for operation/process/object
		'df'.
	*/

_PROTOTYPE(void gen_marshall_funcs, (p_type tp, int exported));
	/*	Generate the marshalling functions for the type indicated by
		'tp'. If the type is exported, the functions are exported as
		well, otherwise they are static.
	*/

_PROTOTYPE(void gen_marshall_macros, (p_type formal, p_type actual));
	/*	Generate macros for marshalling of generic types. */

#endif /* __GEN_MARSHALL_H__ */
