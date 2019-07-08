/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

/* D E B U G G I N G   M A C R O */

/* $Id: debug.h,v 1.5 1995/07/31 08:52:44 ceriel Exp $ */

#include "debugcst.h"

#ifdef lint
#undef DEBUG
#endif

#ifdef DEBUG
#define DO_DEBUG(x, y)	((void)((x) && (y)))
#else
#define DO_DEBUG(x, y)
#endif
#endif /* __DEBUG_H__ */
