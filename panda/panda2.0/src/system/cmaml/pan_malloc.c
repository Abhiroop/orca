/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*
 * Exported names:
 *
 * pan_malloc_start()
 * pan_sbrk()
 * pan_malloc()
 * pan_calloc()
 * pan_free()
 */

#include "pan_sys.h"
#include "pan_malloc.h"
#include "pan_system.h"
#include "pan_global.h"

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define KBYTE   1024
#define MBYTE   (KBYTE * KBYTE)

#define MIN_STORE_SIZE     (  4 * MBYTE)
#define MAX_STORE_SIZE     ( 26 * MBYTE)
#define DECREMENT          (100 * KBYTE)

#define NO_MEM_MSG "pan_malloc_start: could create min. heap\n"
#define NO_MEM_MSGLEN  42

/*
 * Low-level memory management:
 *
 *   The CM5's memory management has the undesirable feature that
 *   it wants memory to look the same on all nodes. In particular,
 *   it wants all heaps to have the same size, so all sbrk()
 *   invocations cause global communication. To avoid this, we
 *   install our own malloc() routine that operates on
 *   one large chunk of memory that is allocated once and for all
 *   at program startup time.
 *
 *   I use Dick Grune's malloc package and provide it with my own
 *   pan_sbrk() routine.
 *
 *   The entire heap space is allocated once and for all at Panda
 *   initialisation time (using the system sbrk). After this, the
 *   system sbrk should never be called again. This is a safe assumption,
 *   since nobody except malloc should call sbrk, and we provide our
 *   own malloc.
 *
 *   +-----------+ <--- top        when breakp passes top, we panic.
 *   |           |
 *   |           |
 *   |           |
 *   |           |
 *   |  unused   |
 *   |           |
 *   |           |
 *   |           |
 *   +-----------+ <--- breakp      pan_sbrk() advances this pointer
 *   | used by   |
 *   | malloc    |
 *   +-----------+ <--- bottom
 */


static int started = 0;			/* initialisation flag */

static char *bottom, *top, *breakp;

#ifdef AMOEBA
static char *
sys_sbrk(int size)
{
    extern char *_getblk(int size);
    char *p;

    p = _getblk(size);

    return p ? p : (char *)-1;
}
#else
#define sys_sbrk(size)  sbrk(size)
#endif

/*
 * You can call pan_malloc_start() twice: once to allocate
 * an initial minimal amount of memory to get things going
 * and once more to get as much as the user _really wants_.
 */
void
pan_malloc_start(int size_wanted)
{
    int cursize, extra;
    char *oldbreak;

#ifdef AMOEBA
    /*
     * No second chances on Amoeba. MEMORY_WANTED should be large
     * enough, because we only want to use one segment; therefore
     * only one call to sys_sbrk() is allowed. Act like this is
     * the second call to pan_malloc_start.
     */
    started++;
#endif
    if (++started > 2) {
	return;
    }
    size_wanted *= MBYTE;
    if (size_wanted == 0) {
	size_wanted = (started == 1 ? MIN_STORE_SIZE : MAX_STORE_SIZE);
    }

    cursize = top - bottom;                   /* this is what we have now */
    if (cursize >= size_wanted) {
	return;
    }
    extra = size_wanted - cursize;            /* we want this much more */

    /*
     * Try to get the extra bytes.
     */
    while (cursize + extra >= MIN_STORE_SIZE) {
	oldbreak = (char *)sys_sbrk(extra);
	if ((int)oldbreak != -1) {
	    if (!bottom) {
		breakp = bottom = oldbreak;   /* heap starts here */
	    }
	    top = oldbreak + extra;
	    return;
	}
	extra -= DECREMENT;                   /* be less greedy */
    }

    /*
     * We did not get the minimum amount of memory we need, so we
     * quit. We just quit and try hard not to call malloc, because
     * it will fail miserably.
     */ 
    _exit(18);
}

char *
pan_sbrk(int incr)
{
    char *oldbreakp = breakp;

    if (!started) {
	pan_malloc_start(0);
    }

    if (breakp + incr >= top) {
	printf("%d: pan_sbrk: warning: private heap (%d bytes) exhausted\n",
	       pan_sys_pid, breakp - bottom);
	return (caddr_t)-1;
    }

    breakp += incr;
    return (caddr_t)oldbreakp;
}

void
pan_malloc_stats(void)
{
    int heapused;
 
    if (pan_stats) {
        heapused = breakp - bottom;
        printf("%3d: used %d bytes of heap memory\n", pan_sys_pid, heapused);
    }
}
 
/*
 * Malloc:
 *
 * Panda provides a fast malloc (pan_malloc) that calls the real malloc
 * to allocate space. Note that the "real" malloc is also provided by
 * Panda for reasons described above.
 */

#ifndef NDEBUG
#define MAGICTEST
#endif

#define ALIGNLOG 3	/* System dependent */
#define MAX2LOG	32
#define BLK2LOG	13
#define MIN2LOG  3	/* Should be at least ALIGNLOG */
#define ALIGNBITS ((1 << ALIGNLOG) - 1)
#define ALIGN(n) ((n + ALIGNBITS) & ~ALIGNBITS)

typedef struct Item Item;
typedef struct Header Header;

struct Header {
#ifdef MAGICTEST
#define MAGIC	0xa0b1c2d3
	int	magic;
#endif
	Item	*nxt;
	int	log2sz;
};

/* Make sure that the header size is properly aligned, by putting it in
   a union with a character buffer of the aligned header size.
*/
struct Item {
	union {
		Header	h;
		char	buf[ALIGN(sizeof(Header))];
	} x;
	char	data[1];
};

static Item *entries[MAX2LOG];

static char logsizes[] = {
/* 0 */		0,
/* 1-2 */	1, 1,
/* 3-4 */	2, 2,
/* 5-8 */	3, 3, 3, 3,
/* 9-16 */	4, 4, 4, 4, 4, 4, 4, 4,
/* 17-32 */	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
/* 33-64 */	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
/* 65-128 */	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
/* 129-256 */	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 
		9,
};

void *
pan_malloc(size_t size)
{
	int	pow;
	Item	*i;
	Item	**ep;

	/* First, find the first power of 2 larger than size. */
	if (size <= (1 << 8)) {
		if (size == 0) return 0;
		if (size <= (1 << MIN2LOG)) pow = MIN2LOG;
		else pow = logsizes[size];
	}
	else if (size <= (1 << 16)) {
		pow = (logsizes+1)[((size-1) >> 8)] + 8;
	}
	else if (size <= (1 << 24)) {
		pow = (logsizes+1)[((size-1) >> 16)] + 16;
	}
	else {
		if (size > ((size_t) 1 << (MAX2LOG-1))) {
			pan_panic("cannot allocate chunk of size %lu",
				  (unsigned long) size);
			return 0;
		}
		pow = (logsizes+1)[((size-1) >> 24)] + 24;
	}

	assert(size <= (1 << pow));
	assert((size > (1 << (pow-1))) || pow == MIN2LOG);

	/* See if we can get it from the list of entries of this size. */
	ep = &entries[pow];
	i = *ep;
	if(i) {
		*ep = i->x.h.nxt;
#ifdef MAGICTEST
		assert(i->x.h.magic == 0);
		i->x.h.magic = MAGIC;
#endif
		return  i->data;
	}

	/* If not, compute size to allocate. */
	size = offsetof(Item, data) + (1 << pow);
	/* Note that size does not have to be aligned; it is already. */

	/* If size is less than a threshold, allocate a block of these,
	   if not, allocate just one.
	*/
	if (pow < BLK2LOG) {
		int	n = 1 << (BLK2LOG - pow);
		Item	*retval;

		i = malloc(size << (BLK2LOG - pow));
		if (i == 0) {
			pan_panic("out of memory");
			return 0;
		}

		retval = i;
		i->x.h.log2sz = pow;
#ifdef MAGICTEST
		i->x.h.magic = MAGIC;
#endif
		i->x.h.nxt = (Item *) ((size_t) i + size);
		*ep = i->x.h.nxt;
		for (n -= 1; n; n--) {
			i = i->x.h.nxt;
#ifdef MAGICTEST
			i->x.h.magic = 0;
#endif
			i->x.h.log2sz = pow;
			i->x.h.nxt = (Item *) ((size_t) i + size);
		}
		i->x.h.nxt = 0;
		return retval->data;
	}

	i = malloc(size);
	if (i == 0) {
		pan_panic("out of memory");
		return 0;
	}

	i->x.h.log2sz = pow;
#ifdef MAGICTEST
	i->x.h.magic = MAGIC;
#endif
	return i->data;
}

void
pan_free(void *ptr)
{
	Item	*i;
	Item	**ep;

	if (ptr == 0) return;

	i = (Item *) ((size_t) ptr - offsetof(Item, data));

#ifdef MAGICTEST
	assert (i->x.h.magic == MAGIC);
	i->x.h.magic = 0;
#endif
	ep = &entries[i->x.h.log2sz];
	i->x.h.nxt = *ep;
	*ep = i;
}

void *
pan_realloc(void *ptr, size_t n)
{
	Item	*i;
	size_t	sz;
	void	*new;
	Item	**ep;

	/* ANSI C standard: malloc if ptr == 0 */
	if (ptr == 0) return pan_malloc(n);

	i = (Item *) ((size_t) ptr - offsetof(Item, data));

#ifdef MAGICTEST
	assert(i->x.h.magic == MAGIC);
#endif

	if (n > 0) {
		/* See if there is enough space in this item. */
		sz = 1 << i->x.h.log2sz;
		if(sz >= n) return ptr;

		new = pan_malloc(n);
		if(new == 0) return 0;

		memcpy(new, ptr, sz);
	}
	else	new = 0;

#ifdef MAGICTEST
	i->x.h.magic = 0;
#endif
	/* Free old entry. */
	ep = &entries[i->x.h.log2sz];
	i->x.h.nxt = *ep;
	*ep = i;

	return new;
}

void *
pan_calloc(size_t nelem, size_t elsize)
{
	size_t sz = nelem * elsize;
	void *ptr = pan_malloc(sz);

	if (! ptr) return 0;
	memset(ptr, 0, sz);
	return ptr;
}






