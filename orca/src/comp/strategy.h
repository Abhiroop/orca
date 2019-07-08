/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __STRATEGY_H__
#define __STRATEGY_H__

/* A C C E S S	 P A T T E R N	 G E N E R A T I O N */

/* $Id: strategy.h,v 1.5 1997/05/15 12:03:03 ceriel Exp $ */

#include	"ansi.h"
#include	"def.h"

/* See the paper "Object Distribution in Orca using Compile-Time and Run-time
   Techniques", by Henri E. Bal and M. Frans Kaashoek.
   This file has one entry point: strategy_def, which is called with
   one parameter, a definition. The parameter is a process, operation,
   or function, and the local access pattern is produced for this parameter.
*/

_PROTOTYPE(void strategy_def, (p_def df));
	/*	Computes the access pattern and stores it in df->prc_patstr.
	*/

#endif /* __STRATEGY_H__ */
