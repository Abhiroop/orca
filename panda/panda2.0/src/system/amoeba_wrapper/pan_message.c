#include "pan_sys.h"
#include "pan_sys_amoeba_wrapper.h"
#include "pan_message.h"
#include "pan_sys_pool.h"
#include "assert.h"
#include "string.h"

#define MESSAGE_POOL_INIT 10

static pool_t pool;


static void
empty(pan_msg_p msg)
{
    if ( msg->top - msg->block > SYS_DEFAULT_MESSAGE_SIZE) {
    	pan_free( msg->block);
    	msg->block = (char *) pan_malloc( SYS_DEFAULT_MESSAGE_SIZE);
    	msg->top = msg->block + SYS_DEFAULT_MESSAGE_SIZE;
    }
    msg->data = msg->top;
    msg->data_len = 0;
    assert( aligned( msg->data, UNIVERSAL_ALIGNMENT));
}


static pool_entry_p
create(void)
{
    pan_msg_p msg;

    msg = (pan_msg_p) pan_malloc( sizeof(pan_msg_t));
    msg->block = (char *) pan_malloc( SYS_DEFAULT_MESSAGE_SIZE);
    msg->top = msg->block + SYS_DEFAULT_MESSAGE_SIZE;
    empty( msg);

    return (pool_entry_p) msg;
}


static void
clear(pool_entry_p msg)
{
    pan_free(((pan_msg_p) msg)->block);
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


void
pan_msg_clear(pan_msg_p msg)
{
    empty(msg);
    pan_sys_pool_put(&pool, (pool_entry_p)msg);
}


pan_msg_p
pan_msg_create(void)
{
    return (pan_msg_p) pan_sys_pool_get(&pool);
}


void
pan_msg_empty(pan_msg_p msg)
{
    /* Don't keep large blocks lying around in msg pools */
    empty(msg);
}


void *
pan_msg_push(pan_msg_p msg, int length, int align)
{
    length = do_align( length, UNIVERSAL_ALIGNMENT);

    /* does the bufffer fit in this msg? If not, realloc the buffer */
    if ( msg->data-length < msg->block) {
	char *old_block, *old_data;
	int size;

	old_block = msg->block;
	old_data = msg->data;
	for ( size = 1; size < msg->data_len+length; size <<= 1);
	msg->block = (char *) pan_malloc( size);
	msg->top = msg->block + size;
	msg->data = msg->top - msg->data_len;
	memmove( msg->data, old_data, msg->data_len);
	pan_free( old_block);
    }

    /* now we can safely push the buffer */
    msg->data_len += length;
    msg->data -= length;

    assert( msg->block <= msg->data);
    return (void *)msg->data;
}


void *
pan_msg_pop(pan_msg_p msg, int length, int align)
{
    char *p = msg->data;

    length = do_align( length, UNIVERSAL_ALIGNMENT);
    msg->data_len -= length;
    msg->data += length;

    assert( msg->data_len >= 0);
    assert( msg->data <= msg->top);
    return (void *)p;
}


void *
pan_msg_look(pan_msg_p msg, int length, int align)
{
    assert( msg->data_len >= do_align(length,UNIVERSAL_ALIGNMENT));
    return (void *) msg->data;
}


int
pan_msg_data_len(pan_msg_p msg)
{
    return msg->data_len;
}


void
pan_msg_truncate(pan_msg_p msg, int length)
{
    assert( msg->data_len >= length);
    msg->data_len = length;
}
