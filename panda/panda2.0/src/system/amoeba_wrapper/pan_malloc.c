#include "pan_sys.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

/*#define DEBUG*/
/*#define DUMP*/

#ifndef NDEBUG
#define MARK 0x74696d0a		/* Some random number :-) */
#endif

#ifdef MARK
int pan_malloc_outstanding = 0;
int pan_count_malloc;
int pan_count_realloc;
int pan_count_free;
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

#ifdef DEBUG
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

#ifdef DUMP
    fprintf(stderr, "MAL: malloc(%d) = %d\n", size, (int)p);
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

#ifdef DEBUG
    size += 256;
    pan_malloc_outstanding += size;
    pan_count_malloc++;
#endif

#ifdef MARK
    size += 8;
#endif
    
    p = malloc(size);
    if (!p){
	pan_error("calloc out of memory\n");
    }
    memset(p, 0, size);

#ifdef DUMP
    fprintf(stderr, "MAL: calloc(%d, %d) = %d\n", nelem, elsize, (int)p);
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
#ifdef DUMP
#ifndef MARK
    fprintf(stderr, "MAL: free(%d) = %d\n", 0, (int)ptr);
#endif
#endif

    if (ptr == 0) return;

#ifdef MARK
    ptr = (void *)((char *)ptr - 8);

    assert(*(int *)ptr == MARK);
    *(int *)ptr = MARK - 1;
    pan_malloc_outstanding -= *((int *)ptr + 1);
    pan_count_free++;

#ifdef DUMP
    fprintf(stderr, "MAL: free(%d) = %d\n", *((int *)ptr + 1), (int)ptr);
#endif

#ifndef NDEBUG
    {
	int i, size = *((int *)ptr + 1);

	for(i = 0; i < size / 4; i++){
	    ((int *)ptr)[i] = (int)ptr + 1;
	}
    }
#endif

#endif

    free(ptr);
}

void *
pan_realloc(void *ptr, size_t size)
{
    void *p;

#ifdef DEBUG
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

#ifdef DUMP
    fprintf(stderr, "MAL: realloc(%d, %d) = %d\n", (int)ptr, size, (int)p);
#endif

#ifdef MARK
    assert(ptr == 0 || *(int *)p == MARK - 1);
    *(int *)p = MARK;
    *((int *)p + 1) = size;
    p = (void *)((char *)p + 8);
#endif

    return p;
}


