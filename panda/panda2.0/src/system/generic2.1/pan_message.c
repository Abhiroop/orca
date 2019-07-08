/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <string.h>
#include <assert.h>

#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_message.h"
#include "pan_error.h"
#include "pan_fragment.h"
#include "pan_nsap.h"
#include "pan_comm.h"
#include "pan_sys_pool.h"
#include "pan_malloc.h"
#include "pan_system.h"

static pool_t pool;
 
#define MESSAGE_POOL_INIT 20
 

static pool_entry_p
create(void)
{
    pan_msg_p msg;

    msg = pan_malloc(sizeof(struct pan_msg));

    msg->pm_buffer = pan_malloc(PACKET_SIZE);
    assert(aligned(msg->pm_buffer, UNIVERSAL_ALIGNMENT));

    msg->pm_size = PACKET_SIZE;
    msg->pm_pos = 0;

    pan_sys_pool_mark((pool_entry_p)&msg->pm_fragment, EXTERNAL_ENTRY);

    return (pool_entry_p)msg;
}


static void
clear(pool_entry_p e)
{
    pan_msg_p msg = (pan_msg_p)e;

    pan_msg_empty(msg);
    pan_free(msg->pm_buffer);
    pan_free(msg);
}

void
pan_sys_msg_start(void)
{
    pan_sys_pool_init(&pool, POLICY_NORMAL, MESSAGE_POOL_INIT,
		      create, clear, "Message pool");
}


void
pan_sys_msg_end(void)
{
    pan_sys_pool_clear(&pool);
}



pan_msg_p
pan_msg_create(void)
{
    pan_msg_p msg;

    msg = (pan_msg_p)pan_sys_pool_get(&pool);

    return msg;
}


void
pan_msg_empty(pan_msg_p msg)
{
    msg->pm_pos = 0;
    if (msg->pm_size != PACKET_SIZE) {
	assert(msg->pm_size > PACKET_SIZE);

	pan_free(msg->pm_buffer);
	msg->pm_buffer = pan_malloc(PACKET_SIZE);
	assert(aligned(msg->pm_buffer, UNIVERSAL_ALIGNMENT));

	msg->pm_size = PACKET_SIZE;
    }
}

void
pan_msg_clear(pan_msg_p msg)
{
    pan_msg_empty(msg);
    pan_sys_pool_put(&pool, (pool_entry_p)msg);
}

/*
 * pan_msg_push:
 *                 Push a data part onto a message. Data parts are
 *                 aligned (in theory on 'align', in practice on
 *                 UNIVERSAL_ALIGNMENT) so there can be holes between
 *                 data allocated in different pushes. Also, all pointers
 *                 returned by previous pushes become invalid, because
 *                 the data may be relocated.
 */
void *
pan_msg_push(pan_msg_p msg, int length, int align)
{
    int pos, next;
    void *p;

    pos = do_align(msg->pm_pos, UNIVERSAL_ALIGNMENT);
    next = do_align(pos + length, UNIVERSAL_ALIGNMENT);

    if (next + MAX_TOTAL_HDR_SIZE > msg->pm_size) {
	msg->pm_size = multiple(next + MAX_TOTAL_HDR_SIZE, PACKET_SIZE);
	assert(next + MAX_TOTAL_HDR_SIZE < msg->pm_size);
	assert(msg->pm_size > PACKET_SIZE);

	if (pos == 0) {		/* cheap realloc */
	    pan_free(msg->pm_buffer);
	    msg->pm_buffer = pan_malloc(msg->pm_size);
	    assert(aligned(msg->pm_buffer, UNIVERSAL_ALIGNMENT));
	}else {
	    msg->pm_buffer = pan_realloc(msg->pm_buffer, msg->pm_size);
	    assert(aligned(msg->pm_buffer, UNIVERSAL_ALIGNMENT));
	}
    }

    p = msg->pm_buffer + pos;
    msg->pm_pos = pos + length;	/* Not aligned yet, so append can add */

    return p;
}

/*
 * pan_msg_append:
 *                 Append data to the last data pushed. These parts are
 *                 consecutive, and their alignment is the alignment of
 *                 the first push. The corresponding pop operation should
 *                 specify the total size of the first push and all
 *                 appended data or the size of the last resize operation
 *                 and all appended data. Since the data may be
 *                 relocated, all pointers returned by previous push
 *                 operations are invalid (including the pointer from the
 *                 push operation that is appended on). It is not allowed
 *                 to append on a recieved message without doing a push
 *                 first.
 */
void *
pan_msg_append(pan_msg_p msg, int length)
{
    int next;
    void *p;

    assert(msg->pm_pos >= 0);
    next = do_align(msg->pm_pos + length, UNIVERSAL_ALIGNMENT);

    if (next + MAX_TOTAL_HDR_SIZE > msg->pm_size) {
	msg->pm_size = multiple(next + MAX_TOTAL_HDR_SIZE, PACKET_SIZE);
	assert(next + MAX_TOTAL_HDR_SIZE < msg->pm_size);
	assert(msg->pm_size > PACKET_SIZE);

	msg->pm_buffer = pan_realloc(msg->pm_buffer, msg->pm_size);
	assert(aligned(msg->pm_buffer, UNIVERSAL_ALIGNMENT));
    }

    p = msg->pm_buffer + msg->pm_pos;
    msg->pm_pos += length;	/* Not aligned yet, so next append can add */

    return p;
}

/*
 * pan_msg_resize:
 *                 Resize the last data part pushed. The caller is
 *                 responsible to specify the current length of the data
 *                 part (pushed + append or previous resize). It returns
 *                 the pointer to the whole data part.
 */
void *
pan_msg_resize(pan_msg_p msg, int old_length, int new_length)
{
    int start = msg->pm_pos - old_length;

    assert(start >= 0);
    if (new_length <= old_length) {
	msg->pm_pos = start + new_length;
	assert(msg->pm_pos >= 0);
    } else {
	(void)pan_msg_append(msg, new_length - old_length);
	assert(msg->pm_pos - new_length == start);
    }


    return msg->pm_buffer + start;
}

/*
 * pan_msg_pop:
 *                 Pop data from a message (LIFO). The pointer is aligned
 *                 (see pan_msg_push). After a pop operation, it is not
 *                 possible to append to the previous push operation.
 */
void *
pan_msg_pop(pan_msg_p msg, int length, int align)
{
    int pos;

    /* First align pos, because the last operation may have been an append */
    pos = do_align(msg->pm_pos, UNIVERSAL_ALIGNMENT);
    length = do_align(length, UNIVERSAL_ALIGNMENT);

    msg->pm_pos = pos - length;
    assert(msg->pm_pos >= 0);
    
    return msg->pm_buffer + msg->pm_pos;
}

/*
 * pan_msg_look:
 *                 Same as push, but the data is not removed from the message.
 */
void *
pan_msg_look(pan_msg_p msg, int length, int align)
{
    int pos;

    /* First align pos, because the last operation may have been an append */
    pos = do_align(msg->pm_pos, UNIVERSAL_ALIGNMENT);
    length = do_align(length, UNIVERSAL_ALIGNMENT);

    assert(pos - length >= 0);
    
    return msg->pm_buffer + pos - length;
}

/*
 * pan_msg_data:
 *                 Access a message in FIFO order. Specify the offset in
 *                 the message (starting at zero) and the length and
 *                 alignment of the data pushed (+appended) there. The
 *                 offset of the next part is written in *offset. This
 *                 offset has to be used to access the next part.
 */
void *
pan_msg_data(pan_msg_p msg, int *offset, int length, int align)
{
    int pos = *offset;

    assert(pos >= 0 && pos < msg->pm_pos);
    assert(aligned(pos, UNIVERSAL_ALIGNMENT));
    assert(pos + length <= msg->pm_pos);
    
    length = do_align(length, UNIVERSAL_ALIGNMENT);
    *offset = pos + length;
    /* *offset may be > msg->pm_pos, since pm_pos is not aligned */

    return msg->pm_buffer + pos;
}
    

/*
 * pan_msg_copy:
 *                 Copy a message. The copy is exactly the same as the
 *                 original message, so append calls are possible.
 */
void
pan_msg_copy(pan_msg_p msg, pan_msg_p copy)
{
    if (copy->pm_size != msg->pm_size) {
	/* Could keep larger buffers */
	copy->pm_size = msg->pm_size;
	pan_free(copy->pm_buffer);
	copy->pm_buffer = pan_malloc(copy->pm_size);
    }

    memcpy(copy->pm_buffer, msg->pm_buffer, copy->pm_size);
    copy->pm_pos = msg->pm_pos;
}

/*
 * pan_msg_assemble:
 *                 Assemble a fragment to a message. The first fragment
 *                 is handled specially. If the fragment does not have to
 *                 be preserved, its data part (of size PACKET_SIZE) is
 *                 swapped with the data in the message, and reallocated
 *                 if more fragments will arrive. Otherwise, the message
 *                 data is resized to fit all fragments, and the fragment
 *                 data is copied to the message.
 *
 *                 All other fragments are just copied.
 */
void
pan_msg_assemble(pan_msg_p msg, pan_fragment_p frag, int preserve)
{
    char *data;

#ifndef SINGLE_FRAGMENT
    frag_hdr_p hdr;
    int size;

    assert(aligned(frag->size, UNIVERSAL_ALIGNMENT));
    assert(frag->size >= 0);

    hdr = frag->header;

    if (hdr->flags & PAN_FRAGMENT_FIRST){
	/* Fragment is first part */

	size = hdr->size * PACKET_SIZE;

	if (!frag->owner || preserve) {
	    if (size > msg->pm_size) {
		assert(!(hdr->flags & PAN_FRAGMENT_LAST));
		assert(size >= PACKET_SIZE);

		msg->pm_size = size;

		pan_free(msg->pm_buffer);
		msg->pm_buffer = pan_malloc(msg->pm_size);
		assert(aligned(msg->pm_buffer, UNIVERSAL_ALIGNMENT));
	    }

	    memcpy(msg->pm_buffer, frag->data, frag->size);
	    msg->pm_pos = frag->size;
	}else {
	    data = msg->pm_buffer;
	    if (msg->pm_size != PACKET_SIZE) {
		assert(msg->pm_size > PACKET_SIZE);

		/*
                 * Make sure that fragments always have a buffer of size
                 * PACKET_SIZE.
                 */
		pan_free(data);
		data = pan_malloc(PACKET_SIZE);
	    }

	    msg->pm_buffer = frag->data;
	    msg->pm_pos = frag->size;
	    msg->pm_size = size;
	    assert(msg->pm_size >= PACKET_SIZE);

	    if (size > PACKET_SIZE) {
		/* Resize the buffer so other fragments will fit */
		msg->pm_buffer = pan_realloc(msg->pm_buffer, size);
		assert(aligned(msg->pm_buffer, UNIVERSAL_ALIGNMENT));
	    }

	    frag->data = data;
	    frag->buf_size = PACKET_SIZE;
	    frag->size = 0;
	    frag->header = NULL;
	    assert(frag->owner);
	}
    }else{
	assert(frag->header->size * PACKET_SIZE <= msg->pm_size);
	assert(aligned(frag->size, UNIVERSAL_ALIGNMENT));

	memcpy(msg->pm_buffer + msg->pm_pos, frag->data, frag->size);
	msg->pm_pos += frag->size;
	assert(msg->pm_pos < msg->pm_size);
    }
    assert(aligned(msg->pm_pos, UNIVERSAL_ALIGNMENT));

#else  /* SINGLE_FRAGMENT */

    assert(aligned(frag->size, UNIVERSAL_ALIGNMENT));
    assert(frag->size >= 0);

    if (!frag->owner || preserve) {
	if (frag->buf_size > msg->pm_size) {
	    assert(frag->buf_size >= PACKET_SIZE);
		
	    msg->pm_size = frag->buf_size;

	    pan_free(msg->pm_buffer);
	    msg->pm_buffer = pan_malloc(msg->pm_size);
	    assert(aligned(msg->pm_buffer, UNIVERSAL_ALIGNMENT));
	}

	memcpy(msg->pm_buffer, frag->data, frag->size);
	msg->pm_pos = frag->size;
    }else {
	data = msg->pm_buffer;
	if (msg->pm_size != PACKET_SIZE) {
	    assert(msg->pm_size > PACKET_SIZE);

	    /*
             * Make sure that fragments always have a buffer of size
             * PACKET_SIZE.
             */
	    pan_free(data);
	    data = pan_malloc(PACKET_SIZE);
	}

	msg->pm_buffer = frag->data;
	msg->pm_pos = frag->size;
	msg->pm_size = frag->buf_size;
	assert(msg->pm_size >= PACKET_SIZE);

	frag->data = data;
	frag->buf_size = PACKET_SIZE;
	frag->size = 0;
	assert(frag->owner);
    }
    
#endif /* SINGLE_FRAGMENT */
}

/*
 * pan_msg_fragment:
 *                 Associate a fragment with the message, and fill it
 *                 with the first part of the message data. Also keep a
 *                 copy of the data that might be overwritten by the
 *                 fragment headers.
 */
pan_fragment_p
pan_msg_fragment(pan_msg_p msg, pan_nsap_p nsap)
{
    pan_fragment_p frag;
#ifndef SINGLE_FRAGMENT
    int flags;
#endif
    int pos;

    frag             = &msg->pm_fragment;
    frag->nsap       = nsap;		/* here, so TOTAL_HDR_SIZE succeeds */
    msg->pm_hdr_size = do_align(TOTAL_HDR_SIZE(frag), UNIVERSAL_ALIGNMENT);

    pos = do_align(msg->pm_pos, UNIVERSAL_ALIGNMENT);

    frag->data   = msg->pm_buffer;
    frag->buf_size = msg->pm_size;
    frag->owner  = 0;
#ifdef SINGLE_FRAGMENT
    frag->size   = pos;
#else
    frag->offset = 0;
    frag->size   = MIN(pos, PACKET_SIZE - msg->pm_hdr_size);
    frag->header = (frag_hdr_p)(msg->pm_buffer + frag->size);
#endif

#ifndef SINGLE_FRAGMENT
    flags = PAN_FRAGMENT_FIRST;
    if (frag->size == pos){
	flags |= PAN_FRAGMENT_LAST;
    }else{
	/* Copy the user data that is going to be overwritten to backup */
	assert(msg->pm_hdr_size < MAX_TOTAL_HDR_SIZE);
	memcpy(msg->pm_backup, (char *)frag->header, msg->pm_hdr_size);
    }

    /* Now we can use frag->header */

    frag->header->size   = multiple(pos + msg->pm_hdr_size, 
				    PACKET_SIZE) / PACKET_SIZE;
    assert(frag->header->size >= 1);
    assert(frag->header->size * PACKET_SIZE <= msg->pm_size);

    frag->header->flags  = flags;
#endif

    return frag;
}

/*
 * pan_msg_next:
 *                 Get the next fragment from a message. First restores
 *                 the data that was overwritten by the header of the
 *                 previous fragment. 
 */
pan_fragment_p
pan_msg_next(pan_msg_p msg)
{
#ifdef SINGLE_FRAGMENT
    return NULL;
#else
    pan_fragment_p frag;
    int flags = 0;
    int offset;
    int pos;

    frag = &msg->pm_fragment;

    assert(frag->owner == 0);

    if (frag->header->flags & PAN_FRAGMENT_LAST){
	/* Last fragment already handed out */
	return NULL;
    }

    assert(frag->size == PACKET_SIZE - msg->pm_hdr_size);
    offset = frag->offset + frag->size;	/* next offset */

    /* Restore original data */
    memcpy((char *)frag->header, msg->pm_backup, msg->pm_hdr_size);
    
    pos = do_align(msg->pm_pos, UNIVERSAL_ALIGNMENT);

    frag->data   = msg->pm_buffer + offset;
    frag->offset = offset;
    frag->size   = MIN(pos - offset, PACKET_SIZE - msg->pm_hdr_size);
    frag->owner  = 0;
    frag->header = (frag_hdr_p)(frag->data + frag->size);
    
    if (frag->data + frag->size == msg->pm_buffer + pos){
	flags = PAN_FRAGMENT_LAST;
    }else{
	/* Copy the user data that is going to be overwritten to backup */
	assert(msg->pm_hdr_size < MAX_TOTAL_HDR_SIZE);
	memcpy(msg->pm_backup, (char *)frag->header, msg->pm_hdr_size);
    }

    /* Now we can use frag->header */
    
    frag->header->size   = multiple(pos + msg->pm_hdr_size, 
				    PACKET_SIZE) / PACKET_SIZE;
    assert(frag->header->size >= 1);
    frag->header->flags  = flags;

    return frag;
#endif
}
