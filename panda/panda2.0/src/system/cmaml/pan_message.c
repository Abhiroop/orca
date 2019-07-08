#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_message.h"
#include "pan_error.h"
#include "pan_buffer.h"
#include "pan_fragment.h"
#include "pan_nsap.h"
#include "pan_comm.h"
#include "pan_sys_pool.h"

#include <string.h>

#define CURRENT_BUFFER(msg) ((msg)->pm_buffer[(msg)->pm_index])

static pool_t pool;
 
#define MESSAGE_POOL_INIT 20
 

static pool_entry_p
create(void)
{
    pan_msg_p msg;

    msg = pan_malloc(sizeof(struct pan_msg));

    msg->pm_buffer = pan_malloc(sizeof(pan_sys_buffer_p));
    msg->pm_index  = 0;
    msg->pm_size   = 1;
    msg->pm_used   = 0;

    msg->pm_buffer[0] = pan_sys_buffer_init();

    msg->pm_nr_rel = 0;

    pan_sys_pool_mark((pool_entry_p)&msg->pm_fragment, EXTERNAL_ENTRY);

    return (pool_entry_p)msg;
}


static void
clear(pool_entry_p e)
{
    pan_msg_p msg = (pan_msg_p)e;

    pan_msg_empty(msg);
    pan_sys_buffer_clear(msg->pm_buffer[0]);
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
    int i;
    int well_sized_buffer = -1;		/* Change by RFHH: reuse a buffer, if
					 * it is of the correct size */

    for (i = 0; i <= msg->pm_used; i++) {
	if (pan_sys_buffer_len(msg->pm_buffer[i]) == PACKET_SIZE &&
	    well_sized_buffer == -1) {
			/* Found buffer of correct size. We reuse this one. */
	    well_sized_buffer = i;
	} else {
			/* Clear the other buffers */
	    pan_sys_buffer_clear(msg->pm_buffer[i]);
	}
    }

    if (well_sized_buffer != -1) {
	msg->pm_buffer[0] = msg->pm_buffer[well_sized_buffer];
	pan_sys_buffer_empty(msg->pm_buffer[0]);
    } else {
			/* Buy a new first buffer. This way, the buffer is
			 * resized to PACKET_SIZE. */
	msg->pm_buffer[0] = pan_sys_buffer_init();
    }

    msg->pm_index = 0;
    msg->pm_used  = 0;
}

void
pan_msg_clear(pan_msg_p msg)
{
    int n;

    if (msg->pm_nr_rel > 0) {
	n = --msg->pm_nr_rel;
	msg->pm_func[n](msg, msg->pm_arg[n]);
    }else{
	pan_msg_empty(msg);
	pan_sys_pool_put(&pool, (pool_entry_p)msg);
    }
}

static void
inc_index(pan_msg_p msg)
{
    msg->pm_index++;
    msg->pm_used = msg->pm_index;

    assert(msg->pm_index <= msg->pm_size);

    if (msg->pm_index == msg->pm_size){
	msg->pm_size += 10;
	msg->pm_buffer = (pan_sys_buffer_p *) 
	    pan_realloc(msg->pm_buffer, 
			msg->pm_size * sizeof(pan_sys_buffer_p));
    }
}



void *
pan_msg_push(pan_msg_p msg, int length, int align)
{
    void *p;

    p = pan_sys_buffer_push(CURRENT_BUFFER(msg), length, FREE_SIZE, align);
    if (p == NULL){
	/* XXX will be incremented in inc_index: msg->pm_index++; */
	inc_index(msg);

	CURRENT_BUFFER(msg) = pan_sys_buffer_init();
	p = pan_sys_buffer_push(CURRENT_BUFFER(msg), length, FREE_SIZE, align);
	assert(p);
    }

    return p;
}


void *
pan_msg_pop(pan_msg_p msg, int length, int align)
{
    void *p;

    p = pan_sys_buffer_pop(CURRENT_BUFFER(msg), length, align);
    if (p == NULL){
	/* XXX: bug. Users have pointers to this data
	pan_sys_buffer_clear(CURRENT_BUFFER(msg));
	*/

	msg->pm_index--;
	assert(msg->pm_index >= 0);

	p = pan_sys_buffer_pop(CURRENT_BUFFER(msg), length, align);
	assert(p);
    }

    return p;
}


void *
pan_msg_look(pan_msg_p msg, int length, int align)
{
    void *p;

    p = pan_sys_buffer_look(CURRENT_BUFFER(msg), length, align);
    if (p == NULL){
	/* XXX: bug. Users have pointers to this data
	pan_sys_buffer_clear(CURRENT_BUFFER(msg));
	*/

	msg->pm_index--;
	assert(msg->pm_index >= 0);

	p = pan_sys_buffer_look(CURRENT_BUFFER(msg), length, align);
	assert(p);
    }

    return p;
}


void
pan_msg_copy(pan_msg_p msg, pan_msg_p copy)
{
    int i;

    if (msg->pm_index >= copy->pm_size){
	copy->pm_size   = msg->pm_index + 1;
	copy->pm_buffer = (pan_sys_buffer_p *)
	    pan_realloc(copy->pm_buffer, 
			copy->pm_size * sizeof(pan_sys_buffer_p));
    }

    for(i = 0; i <= msg->pm_index; i++){
	if (i > copy->pm_index){
	    copy->pm_buffer[i] = pan_sys_buffer_init();
	}
	pan_sys_buffer_copy(msg->pm_buffer[i], copy->pm_buffer[i]);
    }
    copy->pm_index = msg->pm_index;
    copy->pm_used = copy->pm_index;
}


void
pan_msg_release_push(pan_msg_p msg, pan_msg_release_f func, void *arg)
{
    int n;

    n = msg->pm_nr_rel;
    if (n == MAX_RELEASE_FUNCS){
	pan_panic("release function slots occupied");
    }

    msg->pm_func[n] = func;
    msg->pm_arg[n]  = arg;

    msg->pm_nr_rel++;
}


void
pan_msg_release_pop(pan_msg_p msg)
{
    assert(msg->pm_nr_rel > 0);
    --msg->pm_nr_rel;
}

void
pan_msg_assemble(pan_msg_p msg, pan_fragment_p fragment, int preserve)
{
    frag_hdr_p hdr;
    
    hdr = fragment->header;

    if (hdr->index == 0){
	assert(msg->pm_index == 0);
    }else{
	assert(hdr->index == msg->pm_index + 1);
	inc_index(msg);
	CURRENT_BUFFER(msg) = pan_sys_buffer_init();
    }
    
    pan_sys_buffer_first(CURRENT_BUFFER(msg), fragment, preserve);
}


pan_fragment_p
pan_msg_fragment(pan_msg_p msg, pan_nsap_p nsap)
{
    pan_fragment_p frag;
    char *buf_data;
    int buf_size;
    int flags;

    frag             = &msg->pm_fragment;
    msg->pm_hdr_size = FRAG_HDR_SIZE + nsap->hdr_size;

    buf_data = pan_sys_buffer_data(msg->pm_buffer[0]);
    buf_size = pan_sys_buffer_size(msg->pm_buffer[0]);

    frag->data = buf_data;
    frag->f_bufsize = pan_sys_buffer_len(msg->pm_buffer[0]);
    frag->size = buf_size;
    frag->owner  = 0;
    frag->header = (frag_hdr_p)(buf_data + frag->size);
    frag->nsap   = nsap;

    flags = PAN_FRAGMENT_FIRST;
    if (msg->pm_index == 0){
	flags |= PAN_FRAGMENT_LAST;
    }
    
    frag->header->index  = 0;
    frag->header->size   = pan_sys_buffer_len(msg->pm_buffer[0]);
    frag->header->flags  = flags;

    return frag;
}


pan_fragment_p
pan_msg_next(pan_msg_p msg)
{
    pan_fragment_p frag;
    int buf_size, buf_index;
    char *buf_data;
    int flags = 0;

    frag = &msg->pm_fragment;

    if (frag->header->flags & PAN_FRAGMENT_LAST){
	/* Last fragment already handed out */
	return NULL;
    }

    buf_index  = frag->header->index + 1;

    buf_data = pan_sys_buffer_data(msg->pm_buffer[buf_index]);
    buf_size = pan_sys_buffer_size(msg->pm_buffer[buf_index]);
    assert(buf_size >= 0);

    frag->data   = buf_data;
    frag->f_bufsize = pan_sys_buffer_len(msg->pm_buffer[buf_index]);
    frag->size   = buf_size;
    frag->owner  = 0;
    frag->header = (frag_hdr_p)(buf_data + frag->size);
    
    if (msg->pm_index == buf_index){
	flags |= PAN_FRAGMENT_LAST;
    }
    
    frag->header->index  = buf_index;
    frag->header->size   = frag->f_bufsize;
    frag->header->flags  = flags;

    return frag;
}


