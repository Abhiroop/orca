#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_fragment.h"
#include "pan_message.h"
#include "pan_nsap.h"
#include "pan_comm.h"
#include "pan_sys_pool.h"

#include <string.h>

static pool_t pool;

#define FRAGMENT_POOL_SIZE 20
#define FRAGMENT_POOL_INIT 10


static void
empty(pan_fragment_p frag)
{
    frag->size   = 0;
    frag->owner  = 1;

    frag->header = NULL;
    frag->nsap   = NULL;
}
    
static pool_entry_p 
create(void)
{
    pan_fragment_p frag;

    frag = pan_malloc(sizeof(struct pan_fragment));
    frag->data = pan_malloc(PACKET_SIZE);
    frag->f_bufsize = PACKET_SIZE;

    empty(frag);

    return (pool_entry_p)frag;
}

static void
clear(pool_entry_p e)
{
    pan_fragment_p frag = (pan_fragment_p)e;

    assert(frag->size >= 0);

    if (frag->owner){
	pan_free(frag->data);
    }
	
    frag->data     = NULL;
    frag->f_bufsize = 0;
    frag->size     = -1;
    frag->header   = NULL;
    frag->nsap     = NULL;

    pan_free(frag);
}

void 
pan_sys_fragment_start(void)
{
    pan_sys_pool_init(&pool, POLICY_NORMAL, FRAGMENT_POOL_INIT,
		      create, clear, "Fragment pool");
}

void 
pan_sys_fragment_end(void)
{
    pan_sys_pool_clear(&pool);
}


pan_fragment_p 
pan_fragment_create(void)
{
    pan_fragment_p frag;

    frag = (pan_fragment_p)pan_sys_pool_get(&pool);
    return frag;
}

void
pan_fragment_clear(pan_fragment_p frag)
{
    empty(frag);
    pan_sys_pool_put(&pool, (pool_entry_p)frag);
}

int
pan_fragment_copy(pan_fragment_p frag, pan_fragment_p copy, int preserve)
{
    pan_fragment_t tmp;
    pool_mode_t    tmp_mode;
    
    assert(copy->owner);
    
    if (preserve || !frag->owner){
	if (copy->f_bufsize < frag->f_bufsize) {     /* resize copy? */
	    copy->f_bufsize = frag->f_bufsize;
	    pan_free(copy->data);
	    copy->data = pan_malloc(copy->f_bufsize);
	}

	memcpy(copy->data, frag->data, frag->size + TOTAL_HDR_SIZE(frag));

	copy->size   = frag->size;
	copy->header = (frag_hdr_p)(copy->data + copy->size);
	copy->nsap   = frag->nsap;

	return 1;
    }else{
	tmp = *frag;
	*frag = *copy;
	*copy = tmp;

	/* Restore the mode: mode should stay intact at copy time */
	tmp_mode = frag->pool_mode;
	frag->pool_mode = copy->pool_mode;
	copy->pool_mode = tmp_mode;

	return 0;
    }

    /* not reached */
    return -1;
}

void
pan_fragment_nsap(pan_fragment_p fragment, pan_nsap_p nsap)
{
    /* XXX: build hierarchy of nsaps, and check before assignment */
    fragment->nsap = nsap;
}


void *
pan_fragment_header(pan_fragment_p frag)
{
    return frag->data + frag->size + FRAG_HDR_SIZE;
}

int
pan_fragment_flags(pan_fragment_p frag)
{
    return frag->header->flags;
}

int
pan_fragment_length(pan_fragment_p frag)
{
    return frag->size;
}
