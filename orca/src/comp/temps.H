/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __TEMPS_H__
#define __TEMPS_H__

/*  T E M P O R A R Y	V A R I A B L E S  */

/* $Id: temps.H,v 1.11 1997/05/15 12:03:06 ceriel Exp $ */

#include	"ansi.h"
#include	"type.h"
#include	"node.h"

typedef struct tmp t_tmp, *p_tmp;

/* A "tmp" structure exists for each temporary. The fields are:
   tmp_next	a free list of temporaries is maintained. The tmp_next
		field is used to link the temporaries in this list.
   tmp_scopelink
		per scope, a list of temporaries is maintained. The
		tmp_scopelink is used to link the temporaries in this list.
   tmp_id	an identification of this temporary.
   tmp_type	the base type of this temporary, but it may also be a pointer
		to this type.
   tmp_ispointer
		if this flag is set, the temporary actually is a pointer to
		tmp_type.
   tmp_num	numbering for optimizations.
   tmp_expr	the expression for which this temporary is allocated.
*/

struct tmp {
    p_tmp	tmp_next,
		tmp_scopelink;
    int		tmp_id;
    p_type	tmp_type;
    int		tmp_ispointer;
    int		tmp_num;
    p_node	tmp_expr;
};

#define TMP_FMT	"tmp_%d"

/* ALLOCDEF "tmp" 10 */

_PROTOTYPE(p_tmp newtemp, (p_type tp, int isptr));
	/*	Get a new temporary variable, with type 'tp'. If 'isptr' is set,
		it has type "ptr to 'tp'" instead.
	*/

_PROTOTYPE(void oldtemp, (p_tmp t));
	/*	Release the temporary indicated by 't'.
	*/

_PROTOTYPE(void inittemps, (void));
	/*	Initialize for before allocating temporaries. This routine
		must be called before allocating temporaries for every routine,
		operation or function.
	*/

_PROTOTYPE(p_node get_tmpvar, (p_type tp, int isptr));
	/*	Create a node indicating a new temporary variable, which has
		type 'tp'. If 'isptr' is set, it has type "ptr to 'tp'" instead.
	*/

#endif /* __TEMPS_H__ */
