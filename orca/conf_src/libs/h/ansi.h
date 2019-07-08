/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __ANSI_H__INCLUDED
#define __ANSI_H__INCLUDED

/* T O   A N S I   O R   N O T   T O   A N S I ? */

/* $Id: ansi.h,v 1.2 1995/07/31 09:14:12 ceriel Exp $ */

/* Define the * _PROTOTYPE macro to either expand both of its arguments
   (ANSI prototypes), or only the function name (K&R prototypes).
 */

#if __STDC__
#include <stddef.h>

#define	_PROTOTYPE(function, params)	function params
#define _SIZET		size_t
#define _CONST		const

#else

#define	_PROTOTYPE(function, params)	function()
#define _SIZET		unsigned int
#define _CONST

#endif /* __STDC__ */
#endif /* __ANSI_H__INCLUDED */
