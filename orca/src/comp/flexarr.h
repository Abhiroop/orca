/*
 * (c) copyright 1997 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: flexarr.h,v 1.1 1997/05/15 12:02:05 ceriel Exp $ */

/* Utility for arrays of which you don't know the size in advance.
   flex_init creates the initial data structure, after which you can call
   flex_index with any index that you want to exist. The utility will
   keep track of the maximum index used. When you are done, you can call
   flex_finish, which will delete the structure encapsulating the array
   and return a pointer to the array. The number of elements in the array
   will be put in the parameter, unless it is 0.
   The array itself can be removed with free().
   If you don't care about the indices, but just want to add elements,
   you can use flex_next instead of flex_index.
   flex_clear just resets the maximum index, so that the structure can be
   re-used.
   flex_base returns the base of the current array.
   Be careful with this, because flex_finish, flex_index, and flex_next may
   change it.
*/

#include <stdlib.h>

#include "ansi.h"

#define uint		unsigned int

typedef struct flex {
    void    *ptr;	/* Pointer to array. */
    int	    maxused;	/* Maximum index used. */
    uint    cursz;	/* Current size. */
    size_t  elsz;	/* Element size. */
} t_flex, *p_flex;

_PROTOTYPE(p_flex flex_init, (size_t elsz, int startsize));
_PROTOTYPE(void *flex_finish, (p_flex flex, unsigned int *psz));
_PROTOTYPE(void *flex_index, (p_flex flex, uint ind));

#define flex_clear(f)	((f)->maxused = -1)
#define flex_next(f)	(flex_index(f, f->maxused+1))
#define flex_base(f)	((f)->ptr)
