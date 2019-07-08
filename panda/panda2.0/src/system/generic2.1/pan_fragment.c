/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_fragment.h"
#include "pan_message.h"
#include "pan_nsap.h"
#include "pan_comm.h"
#include "pan_sys_pool.h"
#include "pan_system.h"

#include <string.h>
#include <assert.h>

static pool_t pool;

#define FRAGMENT_POOL_SIZE 20
#define FRAGMENT_POOL_INIT 10


static void
empty(pan_fragment_p frag)
{
    frag->size   = 0;
    frag->owner  = 1;

#ifndef SINGLE_FRAGMENT
    frag->header = NULL;
#endif
    frag->nsap   = NULL;
}
    
static pool_entry_p 
create(void)
{
    pan_fragment_p frag;

    frag = pan_malloc(sizeof(struct pan_fragment));
    frag->data = pan_malloc(PACKET_SIZE);
    frag->buf_size = PACKET_SIZE;

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
    frag->buf_size = 0;
    frag->size     = -1;
#ifndef SINGLE_FRAGMENT
    frag->header   = NULL;
#endif
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
    pool_entry_t frag_head, copy_head;
    pan_fragment_t tmp;
    
    assert(copy->owner);
    
    if (preserve || !frag->owner){
	if(copy->buf_size < frag->buf_size) {
	    copy->buf_size = frag->buf_size;
	    pan_free(copy->data);
	    copy->data = pan_malloc(copy->buf_size);
	}

	memcpy(copy->data, frag->data, frag->size + TOTAL_HDR_SIZE(frag));

	copy->size   = frag->size;
#ifndef SINGLE_FRAGMENT
	copy->header = (frag_hdr_p)(copy->data + copy->size);
#endif
	copy->nsap   = frag->nsap;

	return 1;
    }

    frag_head = *(pool_entry_p)frag;
    copy_head = *(pool_entry_p)copy;
    
    tmp = *frag;
    *frag = *copy;
    *copy = tmp;
    
    *(pool_entry_p)frag = frag_head;
    *(pool_entry_p)copy = copy_head;
    
    return 0;
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
    short      nsap_id;
    char      *data;
    pan_nsap_p nsap;

    data = frag->data;

    /* Get nsap */
    len -= NSAP_HDR_SIZE;
    nsap_id = *(short *)(data + len);
    nsap = pan_sys_nsap_id(nsap_id);

    len -= nsap->comm_hdr_size;			/* Skip communication header */

    if (nsap->type != PAN_NSAP_SMALL) {
	/* Skip user common header and get fragment header */
	len -=	nsap->hdr_size + FRAG_HDR_SIZE;

#ifndef SINGLE_FRAGMENT
	frag->header = (frag_hdr_p)(data + len);
#endif
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
	return frag->data + frag->size + FRAG_HDR_SIZE + frag->nsap->hdr_size;
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
	nsap_offset = frag->size + FRAG_HDR_SIZE + frag->nsap->hdr_size +
		      frag->nsap->comm_hdr_size;
    }
    assert(nsap_offset + NSAP_HDR_SIZE <= PACKET_SIZE);
    *(short *)(frag->data + nsap_offset) = pan_sys_nsap_index(nsap);

    return nsap_offset + NSAP_HDR_SIZE;
}


void *
pan_sys_fragment_comm_hdr_push(pan_fragment_p frag)
{
    if (frag->nsap->type == PAN_NSAP_SMALL) {
	return frag->data + frag->size;
    } else {
	return frag->data + frag->size + FRAG_HDR_SIZE + frag->nsap->hdr_size;
    }
}


pan_nsap_p
pan_sys_fragment_nsap_look(pan_fragment_p frag)
{
    int        nsap_offset;
    short      nsap_id;

    if (frag->nsap->type == PAN_NSAP_SMALL) {
	nsap_offset = frag->size + frag->nsap->comm_hdr_size;
    } else {
	nsap_offset = frag->size + FRAG_HDR_SIZE + frag->nsap->hdr_size +
		      frag->nsap->comm_hdr_size;
    }
    assert(nsap_offset + NSAP_HDR_SIZE <= PACKET_SIZE);
    nsap_id = *(short *)(frag->data + nsap_offset);

    return pan_sys_nsap_id(nsap_id);
}


void *
pan_sys_fragment_comm_hdr_look(pan_fragment_p frag)
{
    if (frag->nsap->type == PAN_NSAP_SMALL) {
	return frag->data + frag->size;
    } else {
	return frag->data + frag->size + FRAG_HDR_SIZE + frag->nsap->hdr_size;
    }
}

void *
pan_fragment_header(pan_fragment_p frag)
{
    return frag->data + frag->size + FRAG_HDR_SIZE;
}

int
pan_fragment_flags(pan_fragment_p frag)
{
#ifndef SINGLE_FRAGMENT
    return frag->header->flags;
#else
    return PAN_FRAGMENT_FIRST | PAN_FRAGMENT_LAST;
#endif
}

int
pan_fragment_length(pan_fragment_p frag)
{
    return frag->size;
}

#ifdef SINGLE_FRAGMENT
/*
 * pan_sys_fragment_resize:
 *                 Resize the data buffer of a fragment. This is
 *                 necessary in the SINGLE_FRAGMENT implementation,
 *                 because a fragment may contain more data than
 *                 PACKET_SIZE. The underlying system supports large
 *                 messages (with reasonable reliability). PACKET_SIZE is
 *                 now only used as a chunk size. Since the size of the
 *                 fragment is not knwon in advance, some form of
 *                 communication has to take place before the real data
 *                 is sent, in which the size is told to the
 *                 receiver. The receiver then resizes the fragment, and
 *                 tells its data pointer to the sender (f.e. indirect by
 *                 means of some port mechanism).
 */
void
pan_sys_fragment_resize(pan_fragment_p frag, int size)
{
    assert(frag->owner);

    size = multiple(size, PACKET_SIZE);

    if (size != frag->buf_size) {
	frag->buf_size = size;
	pan_free(frag->data);
	frag->data = pan_malloc(frag->buf_size);
    }
}
#endif
