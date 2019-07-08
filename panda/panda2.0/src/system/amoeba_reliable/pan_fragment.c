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

    frag = (pan_fragment_p)pan_malloc(sizeof(struct pan_fragment));
    frag->data = (char *)pan_malloc(PACKET_SIZE);

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
	assert(frag->size + TOTAL_HDR_SIZE(frag) <= PACKET_SIZE);
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


				/* Changed from pan_sys_fragment_received
				 * to two functions:
				 * pan_sys_fragment_nsap_pop and
				 * pan_sys_fragment_comm_hdr_pop.
				 * The side-effects after receive are all done
				 * in pan_sys_fragment_nsap_pop.
				 *				RFHH */
pan_nsap_p
pan_sys_fragment_nsap_pop(pan_fragment_p frag, int len)
{
    int        nsap_id;
    char      *data;
    pan_nsap_p nsap;

    data = frag->data;

    /* Get nsap */
    len -= NSAP_HDR_SIZE;
    nsap_id = *(int *)(data + len);
    nsap = pan_sys_nsap_id(nsap_id);

    len -= nsap->comm_hdr_size;			/* Skip communication header */

    if (nsap->type != PAN_NSAP_SMALL) {

	len -=	nsap->hdr_size +		/* Skip user common header */
		sizeof(frag_hdr_t);		/* Get fragment header */

	frag->header = (frag_hdr_p)(data + len);
    }

    frag->nsap = nsap;
    frag->owner = 1;
    frag->size = len;

    return nsap;
}


void *
pan_sys_fragment_comm_hdr_pop(pan_fragment_p frag)
{
    if (frag->nsap->type == PAN_NSAP_SMALL) {
	return frag->data + frag->size;
    } else {
	return frag->data + frag->size + sizeof(frag_hdr_t) +
	       frag->nsap->hdr_size;
    }
}


int
pan_sys_fragment_nsap_push(pan_fragment_p frag)
{
    pan_nsap_p nsap = frag->nsap;
    int        nsap_offset;

    if (frag->nsap->type == PAN_NSAP_SMALL) {
	nsap_offset = frag->size + frag->nsap->comm_hdr_size;
    } else {
	nsap_offset = frag->size + sizeof(frag_hdr_t) + frag->nsap->hdr_size +
		      frag->nsap->comm_hdr_size;
    }
    assert(nsap_offset + NSAP_HDR_SIZE <= PACKET_SIZE);
    *(int*)(frag->data + nsap_offset) = pan_sys_nsap_index(nsap);

    return nsap_offset + NSAP_HDR_SIZE;
}


void *
pan_sys_fragment_comm_hdr_push(pan_fragment_p frag)
{
    if (frag->nsap->type == PAN_NSAP_SMALL) {
	return frag->data + frag->size;
    } else {
	return frag->data + frag->size + sizeof(frag_hdr_t) +
	       frag->nsap->hdr_size;
    }
}


pan_nsap_p
pan_sys_fragment_nsap_look(pan_fragment_p frag)
{
    int        nsap_offset;
    int        nsap_id;

    if (frag->nsap->type == PAN_NSAP_SMALL) {
	nsap_offset = frag->size + frag->nsap->comm_hdr_size;
    } else {
	nsap_offset = frag->size + sizeof(frag_hdr_t) + frag->nsap->hdr_size +
		      frag->nsap->comm_hdr_size;
    }
    assert(nsap_offset + NSAP_HDR_SIZE <= PACKET_SIZE);
    nsap_id = *(int*)(frag->data + nsap_offset);

    return pan_sys_nsap_id(nsap_id);
}


void *
pan_sys_fragment_comm_hdr_look(pan_fragment_p frag)
{
    if (frag->nsap->type == PAN_NSAP_SMALL) {
	return frag->data + frag->size;
    } else {
	return frag->data + frag->size + sizeof(frag_hdr_t) +
	       frag->nsap->hdr_size;
    }
}



char *
pan_sys_fragment_data(pan_fragment_p frag, int *size, int *owner,
		      int *buf_size, int *buf_index, int *flags, int preserve)
{
    *size = frag->size;
    *owner = 0;

    if (!preserve && frag->owner && (frag->header->flags & PAN_FRAGBUF_FIRST)){
	frag->owner = 0;
	*owner      = 1;
    }

    *buf_size  = frag->header->size;
    *buf_index = frag->header->index;
    *flags     = frag->header->flags;

    return frag->data;
}


void *
pan_fragment_header(pan_fragment_p frag)
{
    assert(frag->size + frag->nsap->hdr_size + 
	   FRAG_HDR_SIZE + frag->nsap->comm_hdr_size <= PACKET_SIZE);

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
