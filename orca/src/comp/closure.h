/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __CLOSURE_H__
#define __CLOSURE_H__

/* $Id: closure.h,v 1.6 1997/05/15 12:01:43 ceriel Exp $ */

#include	"ansi.h"
#include	"def.h"

_PROTOTYPE(void closure_main, (void));
	/*	Produce the closures of the access patterns. This results in
		scores for objects declared in processes, and for process
		parameters.
	*/

_PROTOTYPE(void gen_score_calls, (p_def process));
	/*	Generate calls to the score function of the RTS for each object
		variable declared in 'process'.
	*/

_PROTOTYPE(void gen_erocs_calls, (p_def process));
	/*	Generate calls to the erocs function of the RTS for each object
		variable declared in 'process' (before the process exits).
	*/

_PROTOTYPE(char *gen_shargs, (p_def process));
	/*	Generate a descriptor containing scores for the shared process
		arguments and return its name.
	*/

#endif /* __CLOSURE_H__ */
