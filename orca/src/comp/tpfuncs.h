/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: tpfuncs.h,v 1.2 1997/05/15 12:03:17 ceriel Exp $ */

#include	"ansi.h"
#include	"type.h"

/*
   Generation of Orca-type specific functions:
   - comparison
	For every Orca type for which comparison is allowed and for which
	no equivalent in C exists, a routine is generated with the following
	interface:
		int <cmpfunc>(void *a, void *b);
	The function returns 1 if the values are equal, 0 if not equal.

   - assignment
	For every constructed Orca type a routine is generated with the
	following interface:
		void <assignfunc>(void *dst, void *src);

   - free-ing
	For every constructed Orca type a routine is generated with the
	following interface:
		void <freefunc>(void *dst);

   The 'exported' parameter indicates wether the resulting function is
   exported or static.
*/

_PROTOTYPE(void gen_compare_func, (p_type tp, int exported));
_PROTOTYPE(void gen_assign_func, (p_type tp, int exported));
_PROTOTYPE(void gen_free_func, (p_type tp, int exported));
