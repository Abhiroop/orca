/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_global.h"
#include "pan_buffer.h"
#include "pan_fragment.h"
#include "pan_message.h"
#include "pan_error.h"
#include "pan_sys_pool.h"

#include <string.h>

#define aligned(p, align)   ((((long)(p)) & ((align) - 1)) == 0)
#define do_align(p, align) \
    ((p + (align) - 1) & ~((align) - 1))

static pool_t pool;
 
#define BUFFER_POOL_INIT 20

static void
empty(pan_sys_buffer_p buf)
{
    if (buf->b_size != PACKET_SIZE){
	pan_free(buf->b_data);
	buf->b_data = (char *)pan_malloc(PACKET_SIZE);
	buf->b_size = PACKET_SIZE;
    }

    buf->b_index = 0;
}
    

static pool_entry_p
create(void) 
{
    pan_sys_buffer_p buf;

    buf = (pan_sys_buffer_p)pan_malloc(sizeof(pan_sys_buffer_t));

    buf->b_data  = (char *)pan_malloc(PACKET_SIZE);
    assert(aligned(buf->b_data, UNIVERSAL_ALIGNMENT));


    buf->b_index = 0;
    buf->b_size  = PACKET_SIZE;

    return (pool_entry_p)buf;
}

static void
clear(pool_entry_p e)
{
    pan_sys_buffer_p buf = (pan_sys_buffer_p)e;

    pan_free(buf->b_data);
    buf->b_data = NULL;
    pan_free(buf);
}


void
pan_sys_buffer_start(void)
{
    assert(aligned(PACKET_SIZE, UNIVERSAL_ALIGNMENT));

    pan_sys_pool_init(&pool, POLICY_NORMAL, BUFFER_POOL_INIT, create,
		      clear, "Buffer pool");
}


void
pan_sys_buffer_end(void)
{
    pan_sys_pool_clear(&pool);
}


static int
buf_size(int size, int clear)
{
    int div;

    clear = do_align(clear, UNIVERSAL_ALIGNMENT);
    div = size / (PACKET_SIZE - clear);
    if (size % (PACKET_SIZE - clear) != 0){
	div++;
    }
    return div * (PACKET_SIZE - clear) + clear;
}

pan_sys_buffer_p
pan_sys_buffer_init(void) 
{
    pan_sys_buffer_p buf;

    buf = (pan_sys_buffer_p)pan_sys_pool_get(&pool);
    return buf;
}

void
pan_sys_buffer_clear(pan_sys_buffer_p buf)
{
    empty(buf);
    pan_sys_pool_put(&pool, (pool_entry_p)buf);
}

void *
pan_sys_buffer_push(pan_sys_buffer_p buf, int size, int clear, int align)
{
    void *p;

    size  = do_align(size, UNIVERSAL_ALIGNMENT);
    clear = do_align(clear, UNIVERSAL_ALIGNMENT);

    if (buf->b_size - (buf->b_index + size) < clear){
	if (buf->b_index == 0){
	    buf->b_size  = buf_size(size, clear);
	    buf->b_data  = (char *)pan_realloc(buf->b_data, buf->b_size);
	    buf->b_index = size;
	    
	    return buf->b_data;
	}else{
	    return NULL;
	}
    }else{
	p = buf->b_data + buf->b_index;
	buf->b_index += size;

	return p;
    }
}

void *
pan_sys_buffer_pop(pan_sys_buffer_p buf, int size, int align)
{
    if (buf->b_index == 0 && size != 0){
	return NULL;
    }
    
    if (align > UNIVERSAL_ALIGNMENT){
	/* may not be properly aligned */
	pan_panic("alignment");
    }

    size = do_align(size, UNIVERSAL_ALIGNMENT);

    assert(buf->b_index >= size);

    buf->b_index -= size;
    
    return buf->b_data + buf->b_index;
}

void *
pan_sys_buffer_look(pan_sys_buffer_p buf, int size, int align)
{
    if (buf->b_index == 0){
	return NULL;
    }
    
    if (align > UNIVERSAL_ALIGNMENT){
	/* may not be properly aligned */
	pan_panic("alignment");
    }

    size = do_align(size, UNIVERSAL_ALIGNMENT);

    assert(buf->b_index >= size);

    return buf->b_data + buf->b_index - size;
}


void
pan_sys_buffer_copy(pan_sys_buffer_p buf, pan_sys_buffer_p copy)
{
    if (buf->b_size != copy->b_size){
	/* Use free and malloc iso realloc to avoid copying */
	pan_free(copy->b_data);
	copy->b_size = buf->b_size;
	copy->b_data = (char *)pan_malloc(copy->b_size);
    }

    (void)memcpy(copy->b_data, buf->b_data, buf->b_index);
    copy->b_index = buf->b_index;
}


void
pan_sys_buffer_resize(pan_sys_buffer_p buf, int size, int clear)
{
    int new_size;

    clear    = do_align(clear, UNIVERSAL_ALIGNMENT);
    new_size = buf_size(size, clear);

    if (buf->b_size >= new_size){
	return;
    }

    buf->b_size = new_size;

    buf->b_data = (char *)pan_realloc(buf->b_data, new_size);
}

void
pan_sys_buffer_first(pan_sys_buffer_p buf, pan_fragment_p frag, int preserve)
{
    int data_size;
    char *data;
    
    if (!frag->owner || preserve){

	data_size = frag->size;			/* Moved RFHH */
	/* Copy the data from fragment to buffer */
	assert(data_size <= PACKET_SIZE);
	if (frag->header->size > PACKET_SIZE){
	    /* Use free and malloc iso realloc to avoid copying */
	    pan_free(buf->b_data);
	    buf->b_data = (char *)pan_malloc(frag->header->size);

	    buf->b_size = frag->header->size;
	}

	memcpy(buf->b_data, frag->data, data_size);
	buf->b_index = frag->size;
    }else{
	/* Swap the data parts of buffer and fragment */
	assert(buf->b_size == PACKET_SIZE);

	data = buf->b_data;

	data_size = frag->header->size;		/* Added RFHH */
	buf->b_data = frag->data;
	if (frag->header->size > PACKET_SIZE){
	    buf->b_data = (char *)pan_realloc(buf->b_data, data_size);
	    buf->b_size  = data_size;		/* Use var because expr freed */
	}
	assert(buf->b_data);
	buf->b_index = frag->size;

	frag->data    = data;
	frag->size    = 0;
	frag->header  = NULL;
    }

    assert(buf->b_index <= buf->b_size);
}

void
pan_sys_buffer_append(pan_sys_buffer_p buf, pan_fragment_p frag)
{
    int size;

    size = frag->size;

    assert(buf->b_index + size <= buf->b_size);
    assert(buf->b_index > 0);

    memcpy(buf->b_data + buf->b_index, frag->data, size);
    buf->b_index += size;
}

char *
pan_sys_buffer_offset(pan_sys_buffer_p buf, int offset, int *size, 
		      int clear, int *last)
{
    char *data;

    assert(offset < buf->b_index);

    clear = do_align(clear, UNIVERSAL_ALIGNMENT);

    data  = buf->b_data + offset;
    *size = buf->b_index - offset;

    if (*size <= PACKET_SIZE - clear){
	*last = 1;
    }else{
	*size = PACKET_SIZE - clear;
	*last = 0;
    }
    assert(aligned(*size, UNIVERSAL_ALIGNMENT));

    return data;
}
