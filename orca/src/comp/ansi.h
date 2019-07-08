/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __ANSI_H__
#define __ANSI_H__

/* T O	 A N S I   O R	 N O T	 T O   A N S I ? */

/* $Id: ansi.h,v 1.5 1997/05/15 12:01:33 ceriel Exp $ */

/* Define the * _PROTOTYPE macro to either expand both of its arguments
   (ANSI prototypes), or only the function name (K&R prototypes).
 */

#if __STDC__
#include	<stdlib.h>
#include	<string.h>
#include	<stdarg.h>

#define	_PROTOTYPE(function, params) \
		function params

#else
#include	<varargs.h>

#define	_PROTOTYPE(function, params) \
		function()

#endif /* __STDC__ */
#endif /* __ANSI_H__ */
