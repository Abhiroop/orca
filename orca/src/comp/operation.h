/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __OPERATION_H__
#define __OPERATION_H__

/* $Id: operation.h,v 1.5 1997/05/15 12:02:38 ceriel Exp $ */

#include	"ansi.h"
#include	"def.h"

_PROTOTYPE(void chk_blocking, (p_def df));
	/*	Analyzes the blocking behavior of operation/function 'df'.
		Records on which operations/functions this depends.
	*/

_PROTOTYPE(void chk_writes, (p_def df));
	/*	Checks an operation 'df' for its effect on the object,
		and sets flags according to the result. Also records on
		which operations/functions this depends.
	*/

#endif /* __OPERATION_H__ */
