/* Author:         Tim Ruhl
 *
 * date:           Wed Nov  6 10:15:39 MET 1996
 *
 * sync:
 *                 Provides a synchronization mechanism between two
 *                 processes.
 *
 */

#include "sync.h"
#include "pan_sys.h"

#include <stdio.h>
#include <assert.h>

#define MAX_SYNCS 20

typedef enum {
    STATE_FREE, STATE_ACQUIRED, STATE_WAITING, STATE_SIGNALED
}state_t;

typedef struct sync{
    state_t     state;
    pan_cond_p  cond;
}sync_t, *sync_p;

static pan_mutex_p lock;
static sync_t sync[MAX_SYNCS];
static int next;

void 
rts_sync_init(void)
{
    int i;

    lock = pan_mutex_create();

    for(i = 0; i < MAX_SYNCS; i++){
	sync[i].state = STATE_FREE;
	sync[i].cond = pan_cond_create(lock);
    }
    
    next = 0;
}

void 
rts_sync_end(void)
{
    int i;

    for(i = 0; i < MAX_SYNCS; i++){
	pan_cond_clear(sync[i].cond);
    }

    pan_mutex_clear(lock);
}

unsigned short 
rts_sync_get(void)
{
    int i, id;

    pan_mutex_lock(lock);

    for(i = 0; i < MAX_SYNCS; i++){
	id = next;
	next = (next + 1) % MAX_SYNCS;

	if (sync[id].state == STATE_FREE){
	    sync[id].state = STATE_ACQUIRED;
	    break;
	}
    }

    if (i == MAX_SYNCS){
	fprintf(stderr, "Not enough sync entries\n");
	assert(0);
    }

    pan_mutex_unlock(lock);

    assert(id >= 0 && id < MAX_SYNCS);
    return id;
}

void           
rts_sync_signal(unsigned short id)
{
    assert(id < MAX_SYNCS);

    pan_mutex_lock(lock);

    switch(sync[id].state){
    case STATE_FREE:
    case STATE_SIGNALED:
	fprintf(stderr, "Illegal sync id\n");
	assert(0);
	break;
    case STATE_ACQUIRED:
	sync[id].state = STATE_SIGNALED;
	break;
    case STATE_WAITING:
	sync[id].state = STATE_SIGNALED;
	pan_cond_signal(sync[id].cond);
	break;
    default:
	fprintf(stderr, "Unknown sync state\n");
	break;
    }

    pan_mutex_unlock(lock);
}

void
rts_sync_wait(unsigned short id)
{
    assert(id < MAX_SYNCS);

    pan_mutex_lock(lock);

    switch(sync[id].state){
    case STATE_FREE:
    case STATE_WAITING:
	fprintf(stderr, "Illegal sync id\n");
	assert(0);
	break;
    case STATE_ACQUIRED:
	sync[id].state = STATE_WAITING;
	while(sync[id].state == STATE_WAITING){
	    pan_cond_wait(sync[id].cond);
	}
	assert(sync[id].state == STATE_SIGNALED);
	sync[id].state = STATE_FREE;
	break;
    case STATE_SIGNALED:
	sync[id].state = STATE_FREE;
	break;
    default:
	fprintf(stderr, "Unknown sync state\n");
	break;
    }

    pan_mutex_unlock(lock);
}
