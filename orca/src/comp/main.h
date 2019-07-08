/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __MAIN_H__
#define __MAIN_H__

/* S O M E   G L O B A L   V A R I A B L E S */

/* $Id: main.h,v 1.9 1997/05/15 12:02:25 ceriel Exp $ */

#include	<stdio.h>
#include	"ansi.h"

extern struct def *CurrDef;	/* definition structure of module or object type
				   defined in current compilation unit.
				*/

extern char	**DEFPATH;	/* search path for specifications. */
extern FILE	*fh, *fc;	/* File descriptors for code generation. */

#endif /* __MAIN_H__ */
