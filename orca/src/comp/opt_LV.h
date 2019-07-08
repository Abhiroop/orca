/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* L I V E   V A R I A B L E S	 A N D	 C O M B I N I N G */

/* $Id: opt_LV.h,v 1.3 1997/05/15 12:02:40 ceriel Exp $ */

#include "ansi.h"
#include "def.h"

/* This optimizer pass is used to combine temporaries. The compiler produces
   many temporaries, and there are many opportunities to combine them.
   The C compiler could combine most of those as well, but temporaries that
   contain more complex values are only freed at the end of a procedure,
   so in that case the C compiler won't notice that it can combine those.
 
   The algorithm of this optimizer phase is as follows:
   - first compute for all variables X the program points P where they are,
     "live", t.i. wether the value of X at P could be used along some
     path in the flow-graph starting at P.
   - then, we can combine two variables when they have the same type and
     their "live" ranges are disjunct.
 
   For now, we only do this for temporaries.
   Future possible extension: also do this for Orca program variables.
*/

_PROTOTYPE(void do_LV, (p_def fdf));
	/*	Perform the optimization on function/operation/process fdf.
	*/
