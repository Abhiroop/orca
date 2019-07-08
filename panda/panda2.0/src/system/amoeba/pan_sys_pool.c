/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <string.h>

#include "pan_sys_pool.h"
#include "pan_error.h"
#include "pan_sync.h"

#ifdef STATISTICS
static int statistics = 0;
#endif

void
pan_sys_pool_start(void)
{
}

void
pan_sys_pool_end(void)
{
}

void
pan_sys_pool_init(pool_p p, policy_t policy, int size, 
		  create_f create, clear_f clear, char *name)
{
    pool_entry_p e;
    int i;

    p->lock   = pan_mutex_create();
    p->create = create;
    p->clear  = clear;
    p->policy = policy;

    p->head   = NULL;
    for(i = 0; i < size; i++){
	e = p->create();
	e->pool_next = p->head;
	e->pool_mode = IN_POOL_ENTRY;
	p->head = e;
    }
    p->nr  = size;
    p->out = 0;

#ifdef STATISTICS
    p->name = pan_malloc(strlen(name) + 1);
    strcpy(p->name, name);
    
    p->nr_created = size;
    p->nr_cleared = 0;
    p->nr_get     = 0;
    p->nr_put     = 0;
#endif
}

void 
pan_sys_pool_clear(pool_p p)
{
    pool_entry_p e;
    int i;

    pan_mutex_lock(p->lock);
    
#ifdef STATISTICS
    if (statistics){
	printf("%d) Pool %s: created %d cleared %d (%d + %d) "
	       "get %d put %d %s\n", pan_my_pid(),
	       p->name, p->nr_created, p->nr_cleared + p->nr,
	       p->nr_cleared, p->nr, p->nr_get, p->nr_put,
	       p->out > 0 ? "EXTERNAL" : "");
    }
    pan_free(p->name);
#endif

    for(i = 0; i < p->nr; i++){
	e = p->head;
	assert(e);
	p->head = e->pool_next;
    
	p->clear(e);
    }
    p->nr = 0;

    p->head = (pool_entry_p)1;

    pan_mutex_unlock(p->lock);
    pan_mutex_clear(p->lock);
}

pool_entry_p
pan_sys_pool_get(pool_p p)
{
    pool_entry_p e = NULL;

    pan_mutex_lock(p->lock);

    switch(p->policy){
    case POLICY_NORMAL:
	if (p->nr == 0){
	    e = p->create();
	    e->pool_mode = IN_POOL_ENTRY;
	    e->pool_next = NULL;
#ifdef STATISTICS
	    p->nr_created++;
#endif
	}else{
	    assert(p->head);
	    
	    e = p->head;
	    p->head = e->pool_next;

	    p->nr--;
	}
	break;
    default:
	printf("Unknown pool policy\n");
	break;
    }

    p->out++;
	
#ifdef STATISTICS
    p->nr_get++;
#endif

    assert(e->pool_mode == IN_POOL_ENTRY);
    e->pool_mode = OUT_POOL_ENTRY;

    pan_mutex_unlock(p->lock);

    return e;
}

void
pan_sys_pool_put(pool_p p, pool_entry_p entry)
{
    pan_mutex_lock(p->lock);

    assert(entry->pool_mode == OUT_POOL_ENTRY);
    entry->pool_mode = IN_POOL_ENTRY;

    p->out--;
    assert(p->out >= 0);

    switch(p->policy){
    case POLICY_NORMAL:
	entry->pool_next = p->head;
	p->head = entry;
	p->nr++;
	break;
    default:
	printf("Unknwon pool policy\n");
	break;
    }

#ifdef STATISTICS
    p->nr_put++;
#endif

    pan_mutex_unlock(p->lock);
}

void
pan_sys_pool_statistics(void)
{
#ifdef STATISTICS
    statistics = 1 - statistics;
    printf("Statistics turned %s\n", statistics ? "on" : "off");
#else
    fprintf(stderr, "No statistics provisions compiled\n");
#endif
}
