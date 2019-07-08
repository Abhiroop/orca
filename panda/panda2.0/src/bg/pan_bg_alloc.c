/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_bg.h"
#include "pan_bg_alloc.h"
#include "pan_bg_global.h"

void *
st_alloc(void **phead, int size, int count)
{
    register char *p = NULL;
    
    if (*phead == 0) {
	p = pan_malloc(size * count);
	assert(p);
	((_PALLOC_) p)->_A_next = 0;
	while (--count) {
	    p += size;
	    ((_PALLOC_) p)->_A_next = (_PALLOC_) (p - size);
	}
	*phead = p;
    }
    else{
	p = *phead;
    }
    *phead = (char *) (((_PALLOC_)p)->_A_next);
    return p;
}


