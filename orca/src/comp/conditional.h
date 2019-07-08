/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */
 
/* C O N D I T I O N A L   C O M P I L A T I O N */

#ifndef __CONDITIONAL_H__
#define __CONDITIONAL_H__
 
/* $Id: conditional.h,v 1.2 1998/06/26 10:20:55 ceriel Exp $ */
 
#include "ansi.h"

_PROTOTYPE(void doconditional, (void));
/*	Handle a conditional compilation line.
*/

extern int
	nestlevel,
	in_include;

#define	K_DEFINE	1
#define K_UNDEF		2
#define K_IFDEF		3
#define K_IFNDEF	4
#define K_ELSE		5
#define K_ENDIF		6
#define K_INCLUDE	7

#endif /* __CONDITIONAL_H__ */
