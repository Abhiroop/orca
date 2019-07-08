/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

/*#define MALLOC_DEBUG*/
/*#define MALLOC_DUMP*/

#ifndef NDEBUG
/* #define MARK 0x74696d0a */		/* Some random number :-) */
#endif

#ifdef MARK
static int pan_malloc_outstanding = 0;
static int pan_count_malloc;
static int pan_count_realloc;
static int pan_count_free;
#endif

#ifdef MALLOC_DUMP
static int malloc_dump = 0;
#endif

static void
pan_error(char *fmt, ...)
{
    va_list ap;
 
    va_start(ap, fmt);
    fprintf(stderr, "PAN malloc) ");
    vfprintf(stderr, fmt, ap);
    exit(-2);
}

void *
pan_malloc(size_t size)
{
    void *p;

#ifdef MALLOC_DEBUG
    size += 256;
#endif

#ifdef MARK
    size += 8;
    pan_malloc_outstanding += size;
    pan_count_malloc++;
#endif

    p = malloc(size);

    if (!p){
	pan_error("malloc out of memory\n");
    }

#ifdef MALLOC_DUMP
    if (malloc_dump) fprintf(stderr, "MAL: malloc(%d) = %d\n", size, (int)p);
#endif

#ifdef MARK
    memset(p, 0x72, size);
    *(int *)p = MARK;
    *((int *)p + 1) = size;
    p = (void *)((char *)p + 8);
#endif

    return p;
}

void *
pan_calloc(size_t nelem, size_t elsize)
{
    void *p;
    size_t size = nelem * elsize;

#ifdef MALLOC_DEBUG
    size += 256;
#endif

#ifdef MARK
    size += 8;
    pan_malloc_outstanding += size;
    pan_count_malloc++;
#endif
    
    p = malloc(size);
    if (!p){
	pan_error("calloc out of memory\n");
    }
    memset(p, 0, size);

#ifdef MALLOC_DUMP
    if (malloc_dump){
	fprintf(stderr, "MAL: calloc(%d, %d) = %d\n", nelem, elsize, (int)p);
    }
#endif

#ifdef MARK
    *(int *)p = MARK;
    *((int *)p + 1) = size;
    p = (void *)((char *)p + 8);
#endif

    return p;
}

void  
pan_free(void *ptr)
{
#ifndef MARK
#ifdef MALLOC_DUMP
    if (malloc_dump) fprintf(stderr, "MAL: free(%d) = %d\n", 0, (int)ptr);
#endif
#endif

    if (ptr == 0) return;

#ifdef MARK
    ptr = (void *)((char *)ptr - 8);

    assert(*(int *)ptr == MARK);
    *(int *)ptr = MARK - 1;
    pan_malloc_outstanding -= *((int *)ptr + 1);
    pan_count_free++;

#ifdef MALLOC_DUMP
    if (malloc_dump){
	fprintf(stderr, "MAL: free(%d) = %d\n", *((int *)ptr + 1), (int)ptr);
    }
#endif

    {
	int i, size = *((int *)ptr + 1);

	for(i = 0; i < size / 4; i++){
	    ((int *)ptr)[i] = (int)ptr + 1;
	}
    }
#endif

    free(ptr);
}

void *
pan_realloc(void *ptr, size_t size)
{
    void *p;

#ifdef MALLOC_DEBUG
    size += 256;
#endif

#ifdef MARK
    size += 8;
    pan_malloc_outstanding += size;
    if (ptr){
	pan_count_realloc++;
    }else{
	pan_count_malloc++;
    }
#endif

    if (ptr == 0){
	p = malloc(size);
    }else{
#ifdef MARK
	ptr = (void *)((char *)ptr - 8);
	assert(*(int *)ptr == MARK);
	*(int *)ptr = MARK - 1;
	pan_malloc_outstanding -= *((int *)ptr + 1);
#endif

	p = realloc(ptr, size);
    }

    if (!p){
	pan_error("realloc out of memory\n");
    }

#ifdef MALLOC_DUMP
    if (malloc_dump){
	fprintf(stderr, "MAL: realloc(%d, %d) = %d\n", (int)ptr, size, (int)p);
    }
#endif

#ifdef MARK
    assert(ptr == 0 || *(int *)p == MARK - 1);
    *(int *)p = MARK;
    *((int *)p + 1) = size;
    p = (void *)((char *)p + 8);
#endif

    return p;
}


#ifdef MALLOC_DUMP
int
pan_malloc_dump(int dump)
{
    int ret = malloc_dump;

    malloc_dump = dump;

    return ret;
}
#endif

#ifdef MARK
void
pan_malloc_statistics(void)
{
    printf("Pan add outstanding: %d malloc: %d realloc: %d free: %d\n", 
	   pan_malloc_outstanding, pan_count_malloc, 
	   pan_count_realloc, pan_count_free);
}
#endif
