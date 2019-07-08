/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_CACHE_
#define _SYS_GENERIC_CACHE_

#include "pan_sys.h"

typedef struct entry{
    struct entry *next;		/* Pointer to next entry */
}entry_t, *entry_p;


typedef struct{
    int         max;		/* Maximum number of entries in the cache */
    int         nr;		/* Number of entries in the cache */
    entry_p     head;		/* Head of cache list */
    pan_mutex_p lock;		/* Lock to protect cache list */
}cache_t, *cache_p;

extern void pan_sys_cache_start(void);
extern void pan_sys_cache_end(void);

extern void pan_sys_cache_init(cache_p cache, int max);
extern void pan_sys_cache_clear(cache_p cache);
extern entry_p pan_sys_cache_get(cache_p cache);
extern int pan_sys_cache_put(cache_p cache, entry_p entry);

#endif
