/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"
#include "debug/pan_sys.h"
#include "pan_system.h"
#include "pan_malloc.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

/*#define MALLOC_DEBUG*/

#ifndef NDEBUG
#define MARK 0x74696d0a		/* Some random number :-) */
#endif

#ifdef MARK
static int pan_malloc_outstanding = 0;
static int pan_count_malloc;
static int pan_count_realloc;
static int pan_count_free;
static int malloc_dump = 0;

static pan_mutex_p lock = NULL;
#endif


void
pan_sys_malloc_start(void)
{
#ifdef MARK
    lock = pan_mutex_create();
#endif
}

void
pan_sys_malloc_end(void)
{
#ifdef MARK
    pan_mutex_clear(lock);
#endif
}

void *
pan_malloc(size_t size)
{
    void *p;

#ifdef MALLOC_DEBUG
    size += 256;
#endif

#ifdef MARK
    /* pan_mutex_create calls malloc, see pan_sys_malloc_start */
    if (lock) pan_mutex_lock(lock);

    size += 8;
    pan_malloc_outstanding += size;
    pan_count_malloc++;
#endif

    p = malloc(size);

    if (!p){
	printf("%d: malloc out of memory\n", pan_sys_pid);
    }

#ifdef MARK
    if (malloc_dump) fprintf(stderr, "%d: MAL: %p malloc(%d)\n", 
			     pan_sys_pid, p, size);

    memset(p, 0x72, size);
    *(int *)p = MARK;
    *((int *)p + 1) = size;
    p = (void *)((char *)p + 8);

    if (lock) pan_mutex_unlock(lock);
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
    if (lock) pan_mutex_lock(lock);

    size += 8;
    pan_malloc_outstanding += size;
    pan_count_malloc++;
#endif
    
    p = malloc(size);
    if (!p){
	printf("%d: calloc out of memory\n", pan_sys_pid);
    }
    memset(p, 0, size);

#ifdef MARK
    if (malloc_dump){
	fprintf(stderr, "%d: MAL: %p malloc(%d) == calloc(%d, %d)\n", 
		pan_sys_pid, p, size, nelem, elsize);
    }

    *(int *)p = MARK;
    *((int *)p + 1) = size;
    p = (void *)((char *)p + 8);

    if (lock) pan_mutex_unlock(lock);
#endif

    return p;
}

void  
pan_free(void *ptr)
{
    if (ptr == 0) return;

#ifdef MARK
    if (lock) pan_mutex_lock(lock);

    ptr = (void *)((char *)ptr - 8);

    assert(*(int *)ptr == MARK);
    *(int *)ptr = MARK - 1;
    pan_malloc_outstanding -= *((int *)ptr + 1);
    pan_count_free++;

    if (malloc_dump){
	fprintf(stderr, "%d: MAL: %p free(%d)\n", pan_sys_pid, ptr,
		*((int *)ptr + 1));
    }

    {
	int i, size = *((int *)ptr + 1);

	for(i = 0; i < size / 4; i++){
	    ((int *)ptr)[i] = (int)ptr + 1;
	}
    }

    if (lock) pan_mutex_unlock(lock);
#endif

    free(ptr);
}

void *
pan_realloc(void *ptr, size_t size)
{
    void *p;
#ifdef MARK
    size_t old_size = -1;
#endif
    
#ifdef MALLOC_DEBUG
    size += 256;
#endif

#ifdef MARK
    if (lock) pan_mutex_lock(lock);

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
	old_size = *((int *)ptr + 1);
#endif

	p = realloc(ptr, size);
    }

    if (!p){
	printf("%d: realloc out of memory\n", pan_sys_pid);
    }

#ifdef MARK
    if (malloc_dump){
	if (ptr) fprintf(stderr, "%d: MAL: %p free(%d) == realloc\n", 
			 pan_sys_pid, ptr, old_size); 
	fprintf(stderr, "%d: MAL: %p malloc(%d) == realloc\n", 
		pan_sys_pid, p, size);
    }

    assert(ptr == 0 || *(int *)p == MARK - 1);
    *(int *)p = MARK;
    *((int *)p + 1) = size;
    p = (void *)((char *)p + 8);

    if (lock) pan_mutex_unlock(lock);
#endif

    return p;
}


#ifdef MARK
int
pan_malloc_dump(int dump)
{
    int ret;

    if (lock) pan_mutex_lock(lock);

    ret = malloc_dump;

    malloc_dump = dump;

    if (lock) pan_mutex_unlock(lock);

    return ret;
}

void
pan_malloc_statistics(void)
{
    if (lock) pan_mutex_lock(lock);

    printf("%d: Pan malloc outstanding: %d malloc: %d realloc: %d free: %d\n", 
	   pan_sys_pid, pan_malloc_outstanding, pan_count_malloc, 
	   pan_count_realloc, pan_count_free);

    if (lock) pan_mutex_unlock(lock);
}
#endif
