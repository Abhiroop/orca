/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __MISC_H__
#define __MISC_H__

/* M I S C E L L A N E O U S */

/* $Id: misc.h,v 1.5 1997/05/15 12:02:29 ceriel Exp $ */

#include	"ansi.h"
#include	"idf.h"

#define is_anon_idf(x)		((x)->id_text[0] == '#')
#define id_not_declared(x) \
	do { \
		if (! is_anon_idf(x)) { \
			error("identifier %s not declared", (x)->id_text); \
		} \
	} while (0)

_PROTOTYPE(p_idf gen_anon_idf, (void));
        /*      A new idf is created out of nowhere, to serve as an
                anonymous name.
        */

_PROTOTYPE(char *mk_str, (char *, ...));
	/*	Concatenates all the argument strings and returns the
		(malloced) result. The list of arguments is terminated by
		a NULL pointer.
	*/

_PROTOTYPE(char *get_basename, (char *fn));
	/*	Returns the last level of the path name in 'fn', also
		stripping the last '.' + extension of.
	*/

_PROTOTYPE(char *get_dirname, (char *fn));
	/*	Returns all but the last level of the path name in 'fn'.
	*/

#endif /* __MISC_H__ */
