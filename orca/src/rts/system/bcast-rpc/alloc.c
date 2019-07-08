/* $Id: alloc.c,v 1.6 1996/07/04 08:52:47 ceriel Exp $ */

#include <interface.h>

#ifndef NDEBUG
#define MAGICTEST
#endif

#include "amoeba.h"
#include "module/mutex.h"

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
#define FREE_MAGIC	0x3d2c1ba0
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

static mutex lock;	/* to protect access to the entries array */
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
};


#ifdef AMOEBA
static void *M_alloc(size_t size)
{
	/* Wrapper around malloc to avoid stupid segment count limit in
	   Amoeba.
	*/
	static size_t total;

	total += size;
	if (total > (1 << 20)) {
		void *p = malloc(1<<20);
		free(p);
		total = 0;
	}
	return malloc(size);
}
#else
#define M_alloc(n)	malloc(n)
#endif

void *
m_malloc(size_t size)
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
		pow = logsizes[(size + ((1 << 8) - 1)) >> 8] + 8;
	}
	else if (size <= (1 << 24)) {
		pow = logsizes[(size + ((1 << 16) - 1)) >> 16] + 16;
	}
	else {
		if (size > ((size_t) 1 << (MAX2LOG-1))) {
			m_liberr("Limit exceeded", "out of memory");
			return 0;
		}
		pow = logsizes[(size + ((1 << 24) - 1)) >> 24] + 24;
	}

	assert(size <= (1 << pow));
	assert((size > (1 << (pow-1))) || pow == MIN2LOG);

	/* See if we can get it from the list of entries of this size. */
	mu_lock(&lock);
	ep = &entries[pow];
	i = *ep;
	if(i) {
		*ep = i->x.h.nxt;
		mu_unlock(&lock);
#ifdef MAGICTEST
		assert(i->x.h.magic == FREE_MAGIC);
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

		i = M_alloc(size << (BLK2LOG - pow));
		if (i == 0) {
			mu_unlock(&lock);
			m_liberr("Limit exceeded", "out of memory");
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
			i->x.h.magic = FREE_MAGIC;
#endif
			i->x.h.log2sz = pow;
			i->x.h.nxt = (Item *) ((size_t) i + size);
		}
		i->x.h.nxt = 0;
		mu_unlock(&lock);
		return retval->data;
	}

	/* malloc() is NOT multithread safe in case of user-threads */
	i = M_alloc(size);
	mu_unlock(&lock);
	if (i == 0) {
		m_liberr("Limit exceeded", "out of memory");
		return 0;
	}

	i->x.h.log2sz = pow;
#ifdef MAGICTEST
	i->x.h.magic = MAGIC;
#endif
	return i->data;
}

void
m_free(void *ptr)
{
	Item	*i;
	Item	**ep;

	if (ptr == 0) return;

	i = (Item *) ((size_t) ptr - offsetof(Item, data));

#ifdef MAGICTEST
	assert (i->x.h.magic == MAGIC);
	i->x.h.magic = FREE_MAGIC;
#endif
	mu_lock(&lock);
	ep = &entries[i->x.h.log2sz];
	i->x.h.nxt = *ep;
	*ep = i;
	mu_unlock(&lock);
}

void *
m_realloc(void *ptr, size_t n)
{
	Item	*i;
	size_t	sz;
	void	*new;
	Item	**ep;

	/* ANSI C standard: malloc if ptr == 0 */
	if (ptr == 0) return m_malloc(n);

	i = (Item *) ((size_t) ptr - offsetof(Item, data));

#ifdef MAGICTEST
	assert(i->x.h.magic == MAGIC);
#endif

	if (n > 0) {
		/* See if there is enough space in this item. */
		sz = 1 << i->x.h.log2sz;
		if(sz >= n) return ptr;

		new = m_malloc(n);
		if(new == 0) return 0;

		memcpy(new, ptr, sz);
	}
	else	new = 0;

#ifdef MAGICTEST
	i->x.h.magic = FREE_MAGIC;
#endif
	/* Free old entry. */
	mu_lock(&lock);
	ep = &entries[i->x.h.log2sz];
	i->x.h.nxt = *ep;
	*ep = i;
	mu_unlock(&lock);

	return new;
}
