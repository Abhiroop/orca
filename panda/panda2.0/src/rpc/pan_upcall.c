/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"
#include "pan_upcall.h"

#include <stdlib.h>
#include <assert.h>

#define CHUNK_SIZE 30

static pan_upcall_t **chunk;
static int            chunk_nr;
static int            chunk_size;

static pan_upcall_p free_list;
static pan_mutex_p  lock;

static pan_upcall_t *
create_chunk(void)
{
    pan_upcall_t *ch;
    int i;

    ch = pan_malloc(CHUNK_SIZE * sizeof(pan_upcall_t));

    for(i = 0; i < CHUNK_SIZE; i++){
	ch[i].next = free_list;
	free_list  = &ch[i];
    }

    return ch;
}
    
	
void
pan_rpc_upcall_start(void)
{
    lock = pan_mutex_create();
    
    chunk = pan_malloc(sizeof(pan_upcall_t *));

    chunk[0] = create_chunk();
    chunk_nr = chunk_size = 1;
}

void
pan_rpc_upcall_end(void)
{
    int i;

    for(i = 0; i < chunk_nr; i++){
	pan_free(chunk[i]);
    }
    pan_free(chunk);

    free_list = (pan_upcall_p)-1;

    pan_mutex_clear(lock);
}

pan_upcall_p
pan_rpc_upcall_get(void)
{
    pan_upcall_p p;

    pan_mutex_lock(lock);

    if (!free_list){
	if (chunk_nr == chunk_size){
	    chunk_size += 5;
	    chunk = pan_realloc(chunk, chunk_size * sizeof(pan_upcall_t *));
				/* Modified RFHH: change '+' to '*'
				 * 		  change realloc to pan_realloc
				 */
	}
	
	chunk[chunk_nr++] = create_chunk();
	assert(free_list);
    }

    p = free_list;
    free_list = p->next;

    pan_mutex_unlock(lock);

    return p;
}

void
pan_rpc_upcall_free(pan_upcall_p p)
{
    pan_mutex_lock(lock);

    p->next = free_list;
    free_list = p;

    pan_mutex_unlock(lock);
}
