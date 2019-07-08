/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __BIGGRP_ALLOC_H__
#define __BIGGRP_ALLOC_H__

typedef struct _ALLOC_ {
        struct _ALLOC_ *_A_next;
} *_PALLOC_;


#define st_free(ptr, phead, size)    (((_PALLOC_)ptr)->_A_next = \
				      (_PALLOC_)(*phead), \
				      *((_PALLOC_ *)phead) = \
				      (_PALLOC_) ptr)

extern void *st_alloc(void **pool, int size, int count);

#endif
