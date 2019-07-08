/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __SETS_H__
#define __SETS_H__

/* B I T   S E T   M A N I P U L A T I O N */

/* $Id: sets.h,v 1.5 1995/07/31 08:55:16 ceriel Exp $ */

#include	<alloc.h>
#include	"ansi.h"

typedef unsigned char *p_set;

#define set_ismember(p, i)	(p[(i) >> 3] & (1 << ((i) & 07)))
#define set_putmember(p, i)	(p[(i) >> 3] |= (1 << ((i) & 07)))
#define set_clrmember(p, i)	(p[(i) >> 3] &= ~(1 << ((i) & 07)))
#define set_size(sz)		(((sz) + 7) >> 3)
#define set_create(sz)		((p_set) Malloc(sz))
#define set_free(p, sz)		(free(p))
#define set_copy(d, s, sz)	((void) memcpy(d, s, sz))
#define set_cmp(d, s, sz)	(memcmp(d, s, sz) != 0)

_PROTOTYPE(void set_init, (p_set s, int full, int sz));

/* The following functions return 1 if the result set changed from its
   previous value.
*/
_PROTOTYPE(int set_assign, (p_set result, p_set src, int sz));
_PROTOTYPE(int set_union, (p_set result, p_set s1, p_set s2, int sz));
_PROTOTYPE(int set_intersect, (p_set result, p_set s1, p_set s2, int sz));
_PROTOTYPE(int set_minus, (p_set result, p_set s1, p_set s2, int sz));
_PROTOTYPE(int set_isempty, (p_set s, int sz));

_PROTOTYPE(int set_Xunion, (p_set result, p_set s1, p_set s2, int sz));
/* Computes ~s1 + s2. */
_PROTOTYPE(int set_Xintersect, (p_set result, p_set s1, p_set s2, int sz));
/* Computes ~s1 . s2. */
#endif /* __SETS_H__ */
