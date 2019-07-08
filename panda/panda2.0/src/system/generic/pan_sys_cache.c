/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys_cache.h"
#include "pan_sys.h"

#include <assert.h>

#define TAIL (entry_p)1		/* Illegal pointer at tail */

void
pan_sys_cache_start(void)
{
}

void
pan_sys_cache_end(void)
{
}

void
pan_sys_cache_init(cache_p cache, int max)
{
    cache->max  = max;
    cache->nr   = 0;
    cache->head = TAIL;
    cache->lock = pan_mutex_create();
}

void
pan_sys_cache_clear(cache_p cache)
{
    assert(cache->nr == 0);
    assert(cache->head == TAIL);

    cache->max = -1;
    pan_mutex_clear(cache->lock);
}

entry_p
pan_sys_cache_get(cache_p cache)
{
    entry_p e = TAIL;

    pan_mutex_lock(cache->lock);
    if (cache->nr != 0){
	e = cache->head;
	cache->head = e->next;
	cache->nr--;
    }

    pan_mutex_unlock(cache->lock);

    if (e == TAIL) return NULL;

    e->next = NULL;
    return e;
}

int
pan_sys_cache_put(cache_p cache, entry_p entry)
{
    int ret = 0;

    assert(entry->next == NULL);

    pan_mutex_lock(cache->lock);

    if (cache->nr < cache->max){
	entry->next = cache->head;
	cache->head = entry;
	cache->nr++;
	ret = 1;
    }

    pan_mutex_unlock(cache->lock);

    return ret;
}
