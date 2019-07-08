#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "pan_sys.h"
#include "pan_malloc.h"
#include "pan_parix.h"

#ifdef pan_malloc
#undef pan_malloc
#endif
#ifdef pan_realloc
#undef pan_realloc
#endif
#ifdef pan_calloc
#undef pan_calloc
#endif
#ifdef pan_free
#undef pan_free
#endif

#ifdef PARIX_PowerPC
#  define SAFE_ALIGN 8
#endif

#ifdef NO_MALLOC_ALIGN
#  undef SAFE_ALIGN
#endif



#ifndef SAFE_ALIGN


void *
pan_malloc(size_t n)
{
    void *p;

    p = malloc(n);
    if (p == NULL) {
	pan_panic("malloc(%d) returns NULL\n", n);
    }
    return p;
}

void *
pan_realloc(void *old_p, size_t n)
{
    void *p;

    if (old_p == NULL) {
	return pan_malloc(n);
    } else {
	p = realloc(old_p, n);
    }
    if (p == NULL) {
	pan_panic("realloc returns NULL\n");
    }
    return p;
}

void *
pan_calloc(size_t nelem, size_t elsize)
{
    void *p;

    p = calloc(nelem, elsize);
    if (p == NULL) {
	pan_panic("calloc returns NULL\n");
    }
    return p;
}

void
pan_free(void *p)
{
    free(p);
}


size_t
pan_mallsize(void *p)
{
    return mallsize(p);
}


#else		/* SAFE_ALIGN */

/* Ah well, Parix has two types of alignment:
 * 1. correct alignment (the same kind everybody has)
 * 2. efficient alignment. You may not get this type if you malloc doubles,
 *    and the double starts one word before a page boundary. The penalty for
 *    this type of misalignment may be horrendous (but then, it may not occur,
 *    e.g. when you don't use doubles).
 *    We have no choices: pan_malloc must return double-aligned pointers.
 *    Since free must free the original pointers, the original is stored one
 *    word before the aligned pointer.
 * Ahum.
 */


#define ALIGN_OFFSET	(SAFE_ALIGN - 1)
#define ALIGN_MASK	(~ALIGN_OFFSET)
#define MALLOC_OFFSET	(ALIGN_OFFSET + sizeof(void*) + sizeof(size_t))

void *
pan_malloc(size_t n)
{
    void *p;
    void *orig_p;

    orig_p = malloc(n + MALLOC_OFFSET);
    if (orig_p == NULL) {
	pan_panic("malloc(%d) returns NULL\n", n);
    }

    p = (void*)((long unsigned int)((char *)orig_p + MALLOC_OFFSET) & ALIGN_MASK);
    ((void **)p)[-1] = orig_p;
    ((void **)p)[-2] = (void*)n;	/* Necessary for realloc */

    return p;
}

/* We cannot simply call realloc and then store the new orig_p: the alignment
 * of old_p and p may be different, in which case the contents becomes shifted!
 * In that case, another possibility is to shift the contents of the realloc'd
 * block, but that means 2 copies in stead of one. In the current solution, we
 * loose the possibility to enlarge the block in the heap without copying: who 
 * knows which is best? (i.e. sometimes 2 copies & sometimes no copy vs. always
 * 1 copy.)
 * In both cases, we need the size of the original malloc'ed block, and the
 * 'clean' way to do this is by storing it in the malloc'ed block...
 */
void *
pan_realloc(void *old_p, size_t n)
{
    void *p;
    int   old_n;

    p = pan_malloc(n);
    if (old_p != NULL) {
	old_n = (int)((void **)old_p)[-2];
	memcpy(p, old_p, old_n);
	pan_free(old_p);
    }

    return p;
}

void *
pan_calloc(size_t nelem, size_t elsize)
{
    void *p;
    int   n;

    n = nelem * elsize;
    p = pan_malloc(n);
    if (p == NULL) {
	pan_panic("calloc returns NULL\n");
    }

    memset(p, '\0', n);

    return p;
}

void
pan_free(void *p)
{
    if (p != NULL) {
	free(((void **)p)[-1]);
    }
}


size_t
pan_mallsize(void *p)
{
    return mallsize(((void **)p)[-1]);
}


#endif		/* SAFE_ALIGN */
