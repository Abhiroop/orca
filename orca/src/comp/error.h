/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __ERROR_H__
#define __ERROR_H__

/* E R R O R	R E P O R T I N G */

/* $Id: error.h,v 1.5 1997/05/15 12:01:57 ceriel Exp $ */

#include	"ansi.h"
#include	"LLlex.h"

_PROTOTYPE(void lexerror, (char *, ...));
_PROTOTYPE(void lexwarning, (char *, ...));
	/*	warnings and errors from the lexical analyzer. These routines
		get the position information from FileName and LineNumber.
	*/

_PROTOTYPE(void fatal, (char *, ...));
	/*	Produces an error message for a fatal error and exits.
	*/

_PROTOTYPE(void crash, (char *, ...));
	/*	Produces an error message for an internal compiler error and
		exits (non-gracefully).
	*/

_PROTOTYPE(void debug, (char *, ...));
	/*	Produces debugging output, but only if the 'd' option is set.
	*/

_PROTOTYPE(void error, (char *, ...));
_PROTOTYPE(void warning, (char *, ...));
	/*	Errors and warnings which use the position information from the
		current token (dot).
	*/

_PROTOTYPE(void pos_error, (t_pos *, char *, ...));
_PROTOTYPE(void pos_warning, (t_pos *, char *, ...));
	/*	Errors and warnings which use the position information from
		the parameter.
	*/

extern int	err_occurred;	/* Flag indicating wether a compilation error
				   occurred.
				*/
#endif /* __ERROR_H__ */
